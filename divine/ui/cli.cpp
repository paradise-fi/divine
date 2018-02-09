// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016 Petr Ročkai <code@fixp.eu>
 * (c) 2016 Viktória Vozárová <>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <divine/ui/cli.hpp>
#include <divine/ui/parser.hpp>
#include <divine/mc/bitcode.hpp>
#include <divine/mc/exec.hpp>
#include <divine/vm/vmutil.h>
#include <divine/cc/compile.hpp>
#include <divine/rt/runtime.hpp>
#include <brick-string>
#include <brick-fs>

DIVINE_RELAX_WARNINGS
#include <brick-llvm>
DIVINE_UNRELAX_WARNINGS

namespace divine {
namespace ui {

namespace {

using namespace brick::string;
using bstr = std::vector< uint8_t >;
using seenType = std::set< std::string >;

bstr readContent( const std::string& path, const struct stat& s ) {
    if ( S_ISLNK( s.st_mode ) ) {
        bstr content( 1024 );
        ssize_t ret;
        while ( ( ret = readlink( path.c_str(),
            reinterpret_cast< char * >( content.data() ), content.size() ) ) < 1 )
        {
            if ( errno != ENAMETOOLONG )
                die( "Cannot read content of symlink " + path );
            content.resize( 2 * content.size() );
        }
        content.resize( ret );
        return content;
    }
    else if ( S_ISREG( s.st_mode ) ) {
        std::ifstream f( path, std::ios::binary );
        bstr content( (std::istreambuf_iterator< char >( f )),
                      std::istreambuf_iterator< char >() );
        return content;
    }
    else if ( S_ISDIR( s.st_mode ) ) {
        return {};
    }
    die( "Capturing of sockets, devices and FIFOs is not supported" );
    __builtin_unreachable();
}

bstr packStat( const struct stat& s ) {
    bstr ret( sizeof( __vmutil_stat ) );
    auto *ss = reinterpret_cast< __vmutil_stat * >( ret.data() );
    ss->st_dev = s.st_dev;
    ss->st_ino = s.st_ino;
    ss->st_mode = s.st_mode;
    ss->st_nlink = s.st_nlink;
    ss->st_uid = s.st_uid;
    ss->st_gid = s.st_gid;
    ss->st_rdev = s.st_rdev;
    ss->st_size = s.st_size;
    ss->st_blksize = s.st_blksize;
    ss->st_blocks = s.st_blocks;

    ss->st_atim_tv_sec = s.st_atim.tv_sec;
    ss->st_atim_tv_nsec = s.st_atim.tv_nsec;

    ss->st_mtim_tv_sec = s.st_mtim.tv_sec;
    ss->st_mtim_tv_nsec = s.st_mtim.tv_nsec;

    ss->st_ctim_tv_sec = s.st_ctim.tv_sec;
    ss->st_ctim_tv_nsec = s.st_ctim.tv_nsec;

    return ret;
}

bool visited( const seenType& s, const std::string& path ) {
    return std::find( s.begin(), s.end(), path ) != s.end();
}

std::string changePathPrefix( const std::string& path, const std::string& oldPref,
    const std::string& newPref )
{
    auto p = brick::fs::splitPath( path );
    auto o = brick::fs::splitPath( oldPref );
    if ( !o.empty() && o.back().empty() )
        o.pop_back();
    std::vector< std::string > suf( p.begin() + o.size(), p.end() );
    return brick::fs::joinPath( newPref, brick::fs::joinPath(suf) );
}

std::string noPrefixChange( const std::string& s ) {
    return s;
}

template < typename MountPath, typename See, typename Seen, typename Count, typename Limit >
bool explore( bool follow, MountPath mountPath, See see, Seen seen, Count count,
    Limit limit, mc::BitCode::Env& env, const std::string& oPath )
{
    auto stat = brick::fs::lstat( oPath );
    auto cont = readContent( oPath, *stat );
    auto pStat = packStat( *stat );
    auto path = mountPath( oPath );
    auto iCount = count();

    if ( seen( path ) )
        return false;
    see( path );

    env.emplace_back( "vfs." + fmt( iCount ) + ".name", bstr( path.begin(), path.end() ) );
    env.emplace_back( "vfs." + fmt( iCount ) + ".stat", pStat );
    env.emplace_back( "vfs." + fmt( iCount ) + ".content", cont );
    limit( cont.size() );

    if ( S_ISLNK( stat->st_mode ) ) {
        std::string symPath ( cont.begin(), cont.end() );
        bool absolute = brick::fs::isAbsolute( symPath);
        if ( !absolute ) {
            auto split = brick::fs::splitPath( oPath );
            split.pop_back();
            symPath = brick::fs::joinPath( brick::fs::joinPath( split ), symPath );
        }
        if ( follow && !seen( symPath ) ) {
            auto ex = [&]( const std::string& item ) {
                if ( absolute )
                    return explore( follow, noPrefixChange, see, seen, count,
                        limit, env, item );
                else
                    return explore( follow, mountPath, see, seen, count,
                            limit, env, item );
            };
            brick::fs::traverseDirectoryTree( symPath, ex, []( std::string ){}, ex );
        }
        return false;
    }
    return S_ISDIR( stat->st_mode );
}

} // namespace

void WithBC::process_options()
{
    using bstr = std::vector< uint8_t >;
    int i = 0;

    for ( auto s : _env )
        _bc_env.emplace_back( "env." + fmt( i++ ), bstr( s.begin(), s.end() ) );
    i = 0;
    for ( auto o : _useropts )
        _bc_env.emplace_back( "arg." + fmt( i++ ), bstr( o.begin(), o.end() ) );
    i = 0;
    for ( auto o : _systemopts )
        _bc_env.emplace_back( "sys." + fmt( i++ ), bstr( o.begin(), o.end() ) );
    i = 0;

    _bc_env.emplace_back( "divine.bcname", bstr( _file.begin(), _file.end() ) );

    if ( cc::Compiler::typeFromFile( _file ) != cc::Compiler::FileType::Unknown )
    {
        if ( !_std.empty() )
            _ccopts_final.push_back( { "-std=" + _std } );
        _ccopts_final.push_back( _file );
        for ( auto &o : _ccOpts )
            std::copy( o.begin(), o.end(), std::back_inserter( _ccopts_final ) );
        for ( auto &l : _linkLibs )
            _ccopts_final.push_back( "-l" + l );
    }
}

void WithBC::report_options()
{
    _log->info( "input file: " + _file + "\n", true );

    if ( !_ccopts_final.empty() )
    {
        _log->info( "compile options:\n", true );
        for ( auto o : _ccopts_final )
            _log->info( "  - " + o + "\n", true );
    }

    _log->info( "input options:\n", true ); /* never empty */

    for ( auto e : _bc_env )
    {
        auto &k = std::get< 0 >( e );
        auto &t = std::get< 1 >( e );
        if ( !brick::string::startsWith( "vfs.", k ) )
            /* FIXME quoting */
            _log->info( "  " + k + ": " + std::string( t.begin(), t.end() ) + "\n", true );
    }

    if ( !_lartPasses.empty() )
    {
        _log->info( "lart passes:\n", true );
        for ( auto p : _lartPasses )
            _log->info( "  - " + p + "\n", true );
    }

    if ( _symbolic ) {
        _log->info( "symbolic: 1\n" );
        _log->info( "smt solver: " + _solver + "\n", true );
    }
    if ( _sequential )
        _log->info( "sequential: 1\n", true );
    if ( _disableStaticReduction )
        _log->info( "disable static reduction: 1\n", true );
    if ( !_relaxed.empty() )
        _log->info( "relaxed memory: " + _relaxed + "\n" );
}

void WithBC::setup()
{
    using namespace brick::string;
    using bstr = std::vector< uint8_t >;
    int i = 0;

    process_options();

    std::set< std::string > vfsCaptured;
    size_t limit = _vfsSizeLimit;
    for ( auto vfs : _vfs ) {
        auto ex = [&](const std::string& item) {
            return explore( vfs.followSymlink,
                [&]( const std::string& s) { return changePathPrefix( s, vfs.capture, vfs.mount ); },
                [&]( const std::string& s ) { vfsCaptured.insert( s ); },
                [&]( const std::string& s ) { return visited( vfsCaptured, s); },
                [&]( ) { return i++; },
                [&]( size_t s ) { if ( limit < s ) die( "VFS capture limit reached"); limit -= s;  },
                _bc_env,
                item );
        };

        if ( vfs.capture == vfs.mount ) {
            vfsCaptured.insert( vfs.capture );
        }
        brick::fs::traverseDirectoryTree( vfs.capture, ex, []( std::string ){ }, ex );
    }

    if ( !_stdin.empty() ) {
        std::ifstream f( _stdin, std::ios::binary );
        bstr content( (std::istreambuf_iterator< char >( f )),
                      std::istreambuf_iterator< char >() );
        _bc_env.emplace_back( "vfs.stdin", content );
    }

    auto magic = brick::fs::readFile( _file, 4 );
    auto magicuc = reinterpret_cast< const unsigned char * >( magic.data() );

    if ( magic.size() == 4 && llvm::isBitcode( magicuc, magicuc + magic.size() ) )
        _bc = std::make_shared< mc::BitCode >( _file );
    else if ( cc::Compiler::typeFromFile( _file ) != cc::Compiler::FileType::Unknown )
    {
        cc::Options ccopt;
        cc::Compile driver( ccopt );

        driver.setupFS( rt::each );
        driver.runCC( _ccopts_final );
        _bc = std::make_shared< mc::BitCode >(
            std::unique_ptr< llvm::Module >( driver.getLinked() ),
            driver.context() );
    } else
        throw std::runtime_error( "don't know how to verify file " + _file + " (unknown type)" );

    _bc->environment( _bc_env );
    _bc->autotrace( _autotrace );
    _bc->reduce( !_disableStaticReduction );
    _bc->sequential( _sequential );
    if ( _symbolic )
        _bc->solver( _solver );
    _bc->lart( _lartPasses );
    _bc->relaxed( _relaxed );
}

void WithBC::init()
{
    ASSERT( !_init_done );

    _log->loader( Phase::LART );
    _bc->do_lart();

    if ( !_dump_bc.empty() )
        brick::llvm::writeModule( _bc->_module.get(), _dump_bc );

    _log->loader( Phase::RR );
    _bc->do_rr();
    _log->loader( Phase::Constants );
    _bc->do_constants();
    _log->loader( Phase::Done );
}

void Cc::run()
{
    cc::Compile driver( _drv );
    driver.setupFS( rt::each );
    driver.setupFS( [&]( auto yield )
                    {
                        for ( auto f : _files )
                            yield( f.first, f.second );
                    } );

    for ( auto &x : _passThroughFlags )
        std::copy( x.begin(), x.end(), std::back_inserter( _flags ) );

    std::string firstFile;
    driver.runCC( _flags, [&]( std::unique_ptr< llvm::Module > &&m, std::string name )
            -> std::unique_ptr< llvm::Module >
        {
            bool first;
            if ( (first = firstFile.empty()) )
                firstFile = name;

            if ( _drv.dont_link ) {
                if ( !_output.empty() && !first )
                    die( "CC: Cannot specify --dont-link/-c with -o with multiple input files." );
                driver.writeToFile( _output.empty() ? outputName( name, "bc" ) : _output, m.get() );
                return nullptr;
            }
            return std::move( m );
        } );

    if ( firstFile.empty() )
        die( "CC: You must specify at least one source file." );

    if ( !_drv.dont_link )
        driver.writeToFile( _output.empty() ? outputName( firstFile, "bc" ) : _output );
}

void Info::run()
{
    WithBC::bitcode(); // dump all WithBC messages before our output
    std::cerr << std::endl
              << "DIVINE " << version() << std::endl << std::endl
              << "Available options for " << _file << " are:" << std::endl;
    Run::run();
    std::cerr << "use -o {option}:{value} to pass these options to the program" << std::endl;
}

std::shared_ptr< Interface > make_cli( std::vector< std::string > args )
{
    return std::make_shared< CLI >( args );
}


}
}
