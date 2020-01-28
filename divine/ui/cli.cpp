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
#include <divine/ui/version.hpp>
#include <divine/mc/bitcode.hpp>
#include <divine/mc/exec.hpp>
#include <divine/vm/vmutil.h>
#include <brick-string>
#include <brick-fs>
#include <brick-llvm>
#include <brick-base64>

DIVINE_RELAX_WARNINGS
#include <llvm/BinaryFormat/Magic.h>
DIVINE_UNRELAX_WARNINGS

namespace divine::ui
{

using bstr = std::vector< uint8_t >;
using seenType = std::set< std::string >;

bstr read_content( const std::string& path, const struct stat& s )
{
    if ( S_ISLNK( s.st_mode ) )
    {
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
    else if ( S_ISREG( s.st_mode ) )
    {
        std::ifstream f( path, std::ios::binary );
        bstr content( (std::istreambuf_iterator< char >( f )),
                      std::istreambuf_iterator< char >() );
        return content;
    }
    else if ( S_ISDIR( s.st_mode ) )
    {
        return {};
    }
    die( "Capturing of sockets, devices and FIFOs is not supported" );
    __builtin_unreachable();
}

bstr pack_stat( const struct stat& s )
{
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

bool visited( const seenType& s, const std::string& path )
{
    return std::find( s.begin(), s.end(), path ) != s.end();
}

std::string change_path_prefix( const std::string& path, const std::string& oldPref,
                                const std::string& newPref )
{
    auto p = brq::split_path( path );
    auto o = brq::split_path( oldPref );
    if ( !o.empty() && o.back().empty() )
        o.pop_back();
    std::vector< std::string > result{ newPref };
    std::copy( p.begin() + o.size(), p.end(), std::back_inserter( result ) );
    return brq::normalize_path( brq::join_path( result ) );
}

std::string no_prefix_change( const std::string& s )
{
    return s;
}

template < typename MountPath, typename See, typename Seen, typename Count, typename Limit >
bool explore( bool follow, MountPath mountPath, See see, Seen seen, Count count,
              Limit limit, mc::BCOptions::Env& env, const std::string& oPath )
{
    auto stat = brq::lstat( oPath );

    if ( !stat )
        die( "Failed to stat " + oPath + " during filesystem capture." );

    auto cont = read_content( oPath, *stat );
    auto pStat = pack_stat( *stat );
    auto path = mountPath( oPath );
    auto iCount = count();

    if ( seen( path ) )
        return false;

    auto path_comp = brq::split_path( path );
    for ( const auto &c : path_comp )
        if ( c == ".." )
        {
            std::cerr << "ERROR: Could not capture symlink." << std::endl
                      << "  Link: " << oPath << std::endl
                      << "  Mount point: " << mountPath( "" ) << std::endl
                      << "  Resolves to: " << path << std::endl;
            die( "Filesystem capture failed." );
        }

    see( path );

    env.emplace_back( "vfs." + std::to_string( iCount ) + ".name", bstr( path.begin(), path.end() ) );
    env.emplace_back( "vfs." + std::to_string( iCount ) + ".stat", pStat );
    env.emplace_back( "vfs." + std::to_string( iCount ) + ".content", cont );
    limit( path, cont.size() );

    if ( S_ISLNK( stat->st_mode ) )
    {
        std::string symPath ( cont.begin(), cont.end() );
        bool absolute = brq::is_absolute( symPath );
        if ( absolute )
        {
            auto sym_split = brq::split_path( symPath );
            for ( size_t i = 2; i < sym_split.size(); ++i )
                explore( follow, no_prefix_change, see, seen, count, limit, env,
                         brq::join_path( sym_split.begin(), sym_split.begin() + i ) );
        }
        else
        {
            auto split = brq::split_path( oPath );
            split.pop_back();
            symPath = brq::join_path( brq::join_path( split ), symPath );
        }
        if ( follow && !seen( symPath ) && brq::lstat( symPath ) )
        {
            auto ex = [&]( const std::string& item )
            {
                if ( absolute )
                    return explore( follow, no_prefix_change, see, seen, count, limit, env, item );
                else
                    return explore( follow, mountPath, see, seen, count, limit, env, item );
            };
            brq::traverse_dir_tree( symPath, ex, []( std::string ){}, ex );
        }
        return false;
    }
    return S_ISDIR( stat->st_mode );
}

namespace cc_ns = divine::cc;

void with_bc::process_options()
{
    using bstr = std::vector< uint8_t >;
    int i = 0;

    for ( auto s : _env )
        _bc_opts.bc_env.emplace_back( "env." + std::to_string( i++ ), bstr( s.begin(), s.end() ) );
    i = 0;
    for ( auto o : _useropts )
        _bc_opts.bc_env.emplace_back( "arg." + std::to_string( i++ ), bstr( o.begin(), o.end() ) );
    i = 0;
    for ( auto o : _systemopts )
        _bc_opts.bc_env.emplace_back( "sys." + std::to_string( i++ ), bstr( o.begin(), o.end() ) );
    i = 0;

    _bc_opts.bc_env.emplace_back( "divine.bcname", bstr( _bc_opts.input_file.name.begin(),
                                  _bc_opts.input_file.name.end() ) );

    if ( cc_ns::typeFromFile( _bc_opts.input_file.name ) != cc_ns::FileType::Unknown )
    {
        if ( !_std.empty() )
            _bc_opts.ccopts.push_back( { "-std=" + _std } );
        _bc_opts.ccopts.push_back( _bc_opts.input_file.name );
        std::copy( _cc_opts.begin(), _cc_opts.end(), std::back_inserter( _bc_opts.ccopts ) );
        for ( auto d : _defs )
            _bc_opts.ccopts.push_back( "-D" + d );
        for ( auto &l : _linkLibs )
            _bc_opts.ccopts.push_back( "-l" + l );
    }

    if ( _bc_opts.symbolic && _bc_opts.lamp_config.empty() )
        _bc_opts.lamp_config = "symbolic";
}

template< typename I, typename O >
void quote( I b, I e, O out )
{
    *out++ = '"';
    for ( ; b != e ; ++ b )
        switch ( *b )
        {
            case '\n': *out++ = '\\'; *out++ = 'n'; break;
            case '"': case '\\': *out++ = '\\';
            default: *out++ = *b;
        }
    *out++ = '"';
}

void with_bc::report_options()
{
    _log->info( "input file: " + _bc_opts.input_file.name + "\n", true );

    if ( !_bc_opts.ccopts.empty() )
    {
        _log->info( "compile options:\n", true );
        for ( auto o : _bc_opts.ccopts )
            _log->info( "  - " + o + "\n", true );
    }

    _log->info( "input options:\n", true ); // never empty

    for ( auto e : _bc_opts.bc_env )
    {
        auto &k = std::get< 0 >( e );
        auto &t = std::get< 1 >( e );
        std::string out;
        if ( brq::starts_with( k, "vfs.") )
        {
            out = "!!binary ";
            brick::base64::encode( t.begin(), t.end(), std::back_inserter( out ) );
        }
        else
            quote( t.begin(), t.end(), std::back_inserter( out ) );
        if ( t.empty() )
            out += "\"\"";
        _log->info( "  " + k + ": " + out + "\n", true );
    }

    if ( !_bc_opts.lart_passes.empty() )
    {
        _log->info( "lart passes:\n", true );
        for ( auto p : _bc_opts.lart_passes )
            _log->info( "  - " + p + "\n", true );
    }

    _log->info( "dios config: " + _bc_opts.dios_config + "\n", true );
    _log->info( "lamp config: \"" + _bc_opts.lamp_config + "\"\n", true );

    if ( _bc_opts.symbolic )
        _log->info( "symbolic: 1\n" );
    if ( _bc_opts.leakcheck )
        _log->info( "leak check: [ " + to_string( _bc_opts.leakcheck, true ) + " ]\n", true );

    if ( _bc_opts.svcomp )
        _log->info( "svcomp: 1\n" );

    if ( _bc_opts.sequential )
        _log->info( "sequential: 1\n", true );
    if ( _bc_opts.synchronous )
        _log->info( "synchronous: 1\n", true );
    if ( _bc_opts.static_reduction )
        _log->info( "static reduction: 1\n", true );
    if ( !_bc_opts.relaxed.empty() )
        _log->info( "relaxed memory: " + _bc_opts.relaxed + "\n" );
    if ( _bc_opts.mcsema )
        _log->info( "mcsema: 1\n", true );
}

void with_bc::setup()
{
    using bstr = std::vector< uint8_t >;

    process_options();

    int i = 0;
    std::set< std::string > vfsCaptured;
    size_t limit = _vfs_limit.size;
    for ( auto vfs : _vfs )
    {
        auto ex = [&]( const std::string& item )
        {
            return explore( vfs.followSymlink,
                [&]( const std::string& s )
                {
                    return change_path_prefix( s, vfs.capture, vfs.mount );
                },
                [&]( const std::string& s ) { vfsCaptured.insert( s ); },
                [&]( const std::string& s ) { return visited( vfsCaptured, s ); },
                [&]( ) { return i++; },
                [&]( const std::string &path, size_t s )
                {
                    if ( s >= 1 << 24 )
                        die( path + " is too big. Capturing files over 16MiB is not yet supported." );
                    if ( limit < s )
                        die( "VFS capture limit reached" );
                    limit -= s;
                },
                _bc_opts.bc_env,
                item );
        };

        if ( vfs.capture == vfs.mount )
            vfsCaptured.insert( vfs.capture );

        brq::traverse_dir_tree( vfs.capture, ex, []( std::string ){ }, ex );
    }

    if ( !_stdin.name.empty() )
    {
        std::ifstream f( _stdin.name, std::ios::binary );
        bstr content{ std::istreambuf_iterator< char >( f ),
                      std::istreambuf_iterator< char >() };
        _bc_opts.bc_env.emplace_back( "vfs.stdin", content );
    }

    if ( _bc_opts.dios_config.empty() && _bc_opts.synchronous )
        _bc_opts.dios_config = "sync";
    
    if ( _bc_opts.dios_config.empty() )
        _bc_opts.dios_config = "default";

    _bc = mc::BitCode::with_options( _bc_opts, _cc_driver );
}

void with_bc::init()
{
    ASSERT( !_init_done );

    _log->loader( Phase::DiOS );
    _bc->do_dios();

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

void cc::run()
{
    using namespace cc;
    _driver.setup( _opts );

    std::copy( _passthrough.begin(), _passthrough.end(), std::back_inserter( _flags ) );

    ParsedOpts po = parseOpts( _flags );
    if ( !po.files.empty() )
    {
        if ( po.files[0].is< cc_ns::File >() )
            po.outputFile  = _output.empty() ?
                outputName( po.files[0].get< cc_ns::File >().name, ".bc" ) : _output;
    } else
        die( "CC: You must specify at least one source file." );

    po.toObjectOnly = _opts.dont_link;
    if ( po.toObjectOnly && po.files.size() > 1 && !_output.empty() )
        die( "CC: Cannot specify --dont-link/-c with -o with multiple input files." );

    if ( _opts.dont_link )
    {
        for ( auto path : po.allowedPaths )
            _driver.addDirectory( path );

        for( auto file : po.files )
        {
            if ( !file.is< cc_ns::File >() )
                continue;
            auto f = file.get< cc_ns::File >();
            auto m = _driver.compile( f.name, f.type, po.opts );
            _driver.writeToFile( outputName( f.name, "bc" ), m.get() );
        }
    }
    else
    {
        _driver.build( po );
        _driver.writeToFile( _output.empty() ? outputName( po.outputFile, "bc" ) : _output );
    }
}

void info::run()
{
    with_bc::bitcode(); // dump all with_bc messages before our output
    std::cerr << std::endl
              << "DIVINE " << version() << std::endl << std::endl
              << "Available options for " << _bc_opts.input_file.name << " are:" << std::endl;
    exec::run();
    std::cerr << "use -o {option}:{value} to pass these options to the program" << std::endl;
}

void with_report::setup_report_file()
{
    if ( _report_filename.name.empty() )
        _report_filename.name = outputName( _bc_opts.input_file.name, "report" );

    if ( !_report_unique )
        return;

    std::random_device rd;
    std::uniform_int_distribution< char > dist( 'a', 'z' );
    std::string fn;
    int fd;

    do {
        fn = _report_filename.name + ".";
        for ( int i = 0; i < 6; ++i )
            fn += dist( rd );
        fd = open( fn.c_str(), O_CREAT | O_EXCL, 0666 );
    } while ( fd < 0 );

    _report_filename.name = fn;
    close( fd );
}

void with_report::cleanup()
{
    if ( !_report_filename.name.empty() )
        std::cerr << "a report was written to " << _report_filename.name << std::endl;
}

std::shared_ptr< Interface > make_cli( int argc, const char **argv )
{
    return std::make_shared< CLI >( argc, argv );
}

}
