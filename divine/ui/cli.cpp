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
#include <divine/vm/bitcode.hpp>
#include <divine/vm/run.hpp>
#include <divine/cc/compile.hpp>
#include <divine/rt/runtime.hpp>
#include <brick-string>

#include <runtime/divine/stat.h>

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
    bstr ret( sizeof( _DivineStat ) );
    auto *ss = reinterpret_cast< _DivineStat * >( ret.data() );
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
    Limit limit, vm::BitCode::Env& env, const std::string& oPath )
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

std::string outputName( std::string path )
{
    return brick::fs::replaceExtension( brick::fs::basename( path ), "bc" );
}

void WithBC::setup()
{
    using namespace brick::string;
    vm::BitCode::Env env;
    using bstr = std::vector< uint8_t >;
    int i = 0;
    for ( auto s : _env )
        env.emplace_back( "env." + fmt( i++ ), bstr( s.begin(), s.end() ) );
    i = 0;
    for ( auto o : _useropts )
        env.emplace_back( "arg." + fmt( i++ ), bstr( o.begin(), o.end() ) );
    i = 0;
    for ( auto o : _systemopts )
        env.emplace_back( "sys." + fmt( i++ ), bstr( o.begin(), o.end() ) );
    i = 0;
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
                env,
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
        env.emplace_back( "vfs.stdin", content );
    }

    env.emplace_back( "divine.bcname", bstr( _file.begin(), _file.end() ) );


    try {
        _bc = std::make_shared< vm::BitCode >( _file );
    } catch ( vm::BCParseError &err ) /* probably not a bitcode file */
    {
        cc::Options ccopt;
        cc::Compile driver( ccopt );
        std::vector< std::string > ccopts;
        if ( !_std.empty() )
            ccopts.push_back( { "-std=" + _std } );
        for ( auto &o : _ccOpts )
            std::copy( o.begin(), o.end(), std::back_inserter( ccopts ) );
        ccopts.push_back( _file );

        driver.setupFS( rt::each );
        driver.runCC( ccopts );
        _bc = std::make_shared< vm::BitCode >(
            std::unique_ptr< llvm::Module >( driver.getLinked() ),
            driver.context() );
    }
    _bc->environment( env );
    _bc->autotrace( _autotrace );
    _bc->reduce( !_disableStaticReduction );
    if ( _symbolic )
        _lartPasses.emplace_back( "abstraction:sym" );
    _bc->lart( _lartPasses );
}

void Cc::run()
{
    cc::Compile driver( _drv );
    driver.setupFS( rt::each );
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
                driver.writeToFile( _output.empty() ? outputName( name ) : _output, m.get() );
                return nullptr;
            }
            return std::move( m );
        } );

    if ( firstFile.empty() )
        die( "CC: You must specify at least one source file." );

    if ( !_drv.dont_link )
        driver.writeToFile( _output.empty() ? outputName( firstFile ) : _output );
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

struct CLI : Interface
{
    bool _batch;
    std::vector< std::string > _args;

    CLI( int argc, const char **argv ) :
        _batch( true ), _args( cmd::from_argv( argc, argv ) )
    { }

    CLI( std::vector< std::string > args ) :
        _batch( true ), _args( args )
    { }

    auto validator()
    {
        return cmd::make_validator()->
            add( "file", []( std::string s, auto good, auto bad )
                 {
                     if ( s[0] == '-' ) /* FIXME! zisit, kde sa to rozbije */
                         return bad( cmd::BadFormat, "file must not start with -" );
                     if ( !brick::fs::access( s, F_OK ) )
                         return bad( cmd::BadContent, "file " + s + " does not exist");
                     if ( !brick::fs::access( s, R_OK ) )
                         return bad( cmd::BadContent, "file " + s + " is not readable");
                     return good( s );
                 } ) ->
            add( "vfsdir", []( std::string s, auto good, auto bad )
                {
                    WithBC::VfsDir dir { .followSymlink = true };
                    std::regex sep(":");
                    std::sregex_token_iterator it( s.begin(), s.end(), sep, -1 );
                    int i;
                    for ( i = 0; it != std::sregex_token_iterator(); it++, i++ )
                        switch ( i ) {
                        case 0:
                            dir.capture = *it;
                            dir.mount = *it;
                            break;
                        case 1:
                            if ( *it == "follow" )
                                dir.followSymlink = true;
                            else if ( *it == "nofollow" )
                                dir.followSymlink = false;
                            else
                                return bad( cmd::BadContent, "invalid option for follow links" );
                            break;
                        case 2:
                            dir.mount = *it;
                            break;
                        default:
                            return bad( cmd::BadContent, " unexpected attribute "
                                + std::string( *it ) + " in vfsdir" );
                    }

                    if ( i < 1 )
                        return bad( cmd::BadContent, "missing a directory to capture" );
                    if ( !brick::fs::access( dir.capture, F_OK ) )
                        return bad( cmd::BadContent, "file or directory " + dir.capture + " does not exist" );
                    if ( !brick::fs::access( dir.capture, R_OK ) )
                        return bad( cmd::BadContent, "file or directory " + dir.capture + " is not readable" );
                    return good( dir );
                } ) ->
            add( "mem", []( std::string s, auto good, auto bad )
                {
                    try {
                        size_t size = memFromString( s );
                        return good( size );
                    }
                    catch ( const std::invalid_argument& e ) {
                        return bad( cmd::BadContent, std::string("cannot read size: ")
                            + e.what() );
                    }
                    catch ( const std::out_of_range& e ) {
                        return bad( cmd::BadContent, "size overflow" );
                    }
                } ) ->
            add( "repfmt", []( std::string s, auto good, auto bad )
                {
                    if ( s.compare("none") == 0 )
                        return good( Report::None );
                    if ( s.compare("yaml-long") == 0 )
                        return good( Report::YamlLong );
                    if ( s.compare("yaml") == 0 )
                        return good( Report::Yaml );
                    return bad( cmd::BadContent, s + " is not a valid report format" );
                } ) ->
            add( "label", []( std::string s, auto good, auto bad )
                {
                    if ( s.compare("none") == 0 )
                        return good( Draw::None );
                    if ( s.compare("all") == 0 )
                        return good( Draw::All );
                    if ( s.compare("trace") == 0 )
                        return good( Draw::Trace );
                    return bad( cmd::BadContent, s + " is not a valid label type" );
                } ) ->
            add( "tracepoints", []( std::string s, auto good, auto bad )
                {
                    if ( s == "calls" )
                        return good( vm::AutoTrace::Calls );
                    if ( s == "none" )
                        return good( vm::AutoTrace::Nothing );
                    return bad( cmd::BadContent, s + " is nod a valid tracepoint specification" );
                } ) ->
            add( "paths", []( std::string s, auto good, auto )
                {
                    std::vector< std::string > out;
                    std::regex sep(":");
                    std::sregex_token_iterator it( s.begin(), s.end(), sep, -1 );
                    std::copy( it, std::sregex_token_iterator(), std::back_inserter( out ) );
                    return good( out );
                } ) ->
            add( "commasep", []( std::string s, auto good, auto )
                {
                    std::vector< std::string > out;
                    for ( auto x : brick::string::splitStringBy( s, "[\t ]*,[\t ]*" ) )
                        out.emplace_back( x );
                    return good( out );
                } );
    }

    auto commands()
    {
        auto v = validator();

        auto helpopts = cmd::make_option_set< Help >( v )
            .option( "[{string}]", &Help::_cmd, "print man to specified command"s );

        auto bcopts = cmd::make_option_set< WithBC >( v )
            .option( "[-D {string}|-D{string}]", &WithBC::_env, "add to the environment"s )
            .option( "[-C,{commasep}]", &WithBC::_ccOpts, "options passed to compiler compiler"s )
            .option( "[--autotrace {tracepoints}]", &WithBC::_autotrace, "insert trace calls"s )
            .option( "[-std={string}]", &WithBC::_std, "set the C or C++ standard to use"s )
            .option( "[--disable-static-reduction]", &WithBC::_disableStaticReduction,
                     "disable static (transformation based) state space reductions"s )
            .option( "[--lart {string}]", &WithBC::_lartPasses,
                     "run an additional LART pass in the loader" )
            .option( "[-o {string}|-o{string}]", &WithBC::_systemopts, "system options"s )
            .option( "[--vfslimit {mem}]", &WithBC::_vfsSizeLimit,
                     "filesystem snapshot size limit (default 16 MiB)"s )
            .option( "[--capture {vfsdir}]", &WithBC::_vfs,
                "capture directory in form {dir}[:{follow|nofollow}[:{mount point}]]"s )
            .option( "[--stdin {file}]", &WithBC::_stdin,
                     "capture file and pass it to OS as stdin for verified program" )
            .option( "[--symbolic|--ceds]", &WithBC::_symbolic,
                     "use control-explicit data-symbolic (CEDS) model checking algorithm"s )
            .option( "[--solver {string}]", &WithBC::_solver,
                     "solver command to be used by CEDS algorithms (the solver must accept SMT-LIBv2 queries on standard input and produce results on standard output)"s )
            .option( "{file}", &WithBC::_file, "the bitcode file to load"s,
                  cmd::OptionFlag::Required | cmd::OptionFlag::Final );

        using DrvOpt = cc::Options;
        auto ccdrvopts = cmd::make_option_set< DrvOpt >( v )
            .option( "[-c|--dont-link]", &DrvOpt::dont_link, "do not link"s );

        auto ccopts = cmd::make_option_set< Cc >( v )
            .options( ccdrvopts, &Cc::_drv )
            .option( "[-o {string}]", &Cc::_output, "the name of the output file"s )
            .option( "[-C,{commasep}]", &Cc::_passThroughFlags,
                     "pass additional options to the compiler"s )
            .option( "[{string}]", &Cc::_flags,
                     "any clang options or input files (C, C++, object, bitcode)"s );

        auto vrfyopts = cmd::make_option_set< Verify >( v )
            .option( "[--threads {int}|-T {int}]", &Verify::_threads, "number of threads to use"s )
            .option( "[--max-memory {mem}]", &Verify::_max_mem, "limit memory use"s )
            .option( "[--max-time {int}]", &Verify::_max_time, "maximum allowed run time in seconds"s )
            .option( "[--report {repfmt}|-r {repfmt}]", &Verify::_report, "print a report (yaml, yaml-long or none)"s )
            .option( "[--num-callers {int}]"s, &Verify::_num_callers,
                     "the number of frames to print in a backtrace [default = 10]" );

        auto drawopts = cmd::make_option_set< Draw >( v )
            .option( "[--distance {int}|-d {int}]", &Draw::_distance, "maximum node distance"s )
            .option( "[--render {string}]", &Draw::_render, "the command to process the dot source"s );

        auto dccopts = cmd::make_option_set< DivineCc >( v )
            .option( "[-o {string}]", &DivineCc::_output, "output file"s )
            .option( "[-c]", &DivineCc::_dontLink,
                     "compile or assemble the source files, but do not link."s )
            .option( "[--divinert-path {paths}]", &DivineCc::_libPaths,
                     "paths to DIVINE runtime libraries (':' separated)"s )
            .option( "[{string}]", &DivineCc::_flags,
                     "any clang options or input files (C, C++, object, bitcode)"s );
        auto dldopts = cmd::make_option_set< DivineLd >( v )
            .option( "[-o {string}]", &DivineLd::_output, "output file"s )
            .option( "[-r|-i|--relocable]", &DivineLd::_incremental,
                     "generate incremental/relocable object file"s )
            .option( "[{string}]", &DivineLd::_flags, "any ld options or input files"s );

        auto simopts = cmd::make_option_set< Sim >( v )
            .option( "[--batch]", &Sim::_batch, "execute in batch mode"s );

        auto parser = cmd::make_parser( v )
            .command< Verify >( &WithBC::_useropts, vrfyopts, bcopts )
            .command< Run >( &WithBC::_useropts, bcopts )
            .command< Sim >( &WithBC::_useropts, bcopts, simopts )
            .command< Draw >( drawopts, bcopts )
            .command< Info >( bcopts )
            .command< Cc >( ccopts )
            .command< DivineCc >( dccopts )
            .command< DivineLd >( dldopts )
            .command< Version >()
            .command< Help >( helpopts );
        return parser;
    }

    template< typename P >
    auto parse( P p )
    {
        if ( _args.size() >= 1 )
            if ( _args[0] == "--help" || _args[0] == "--version" )
                _args[0] = _args[0].substr( 2 );
        return p.parse( _args.begin(), _args.end() );
    }

    std::shared_ptr< Interface > resolve() override
    {
        if ( _batch )
            return Interface::resolve();
        else
            return std::make_shared< ui::Curses >( /* ... */ );
    }

    virtual int main() override
    {
        try {
            auto cmds = commands();
            auto cmd = parse( cmds );

            if ( cmd.empty()  )
            {
                Help().run( cmds );
                return 0;
            }

            cmd.match( [&]( Help &help )
                       {
                           help.run( cmds );
                       },
                       [&]( Command &c )
                       {
                           c.setup();
                           c.run();
                       } );
            return 0;
        }
        catch ( brick::except::Error &e )
        {
            std::cerr << "ERROR: " << e.what() << std::endl;
            exception( e );
            return e._exit;
        }
    }
};

std::shared_ptr< Interface > make_cli( std::vector< std::string > args )
{
    return std::make_shared< CLI >( args );
}


}
}
