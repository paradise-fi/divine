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
    bstr content( 11 * sizeof( uint64_t ) /*members*/ +
                  3 * ( sizeof( uint64_t ) + sizeof( uint32_t ) ) /*time*/ );
    uint8_t *d = content.data();
    memcpy( d += 0, &s.st_dev, sizeof( s.st_dev ) );
    memcpy( d += 8, &s.st_ino, sizeof( s.st_ino ) );
    memcpy( d += 8, &s.st_mode, sizeof( s.st_mode ) );
    memcpy( d += 8, &s.st_nlink, sizeof( s.st_nlink ) );
    memcpy( d += 8, &s.st_uid, sizeof( s.st_uid ) );
    memcpy( d += 8, &s.st_gid, sizeof( s.st_gid ) );
    memcpy( d += 8, &s.st_rdev, sizeof( s.st_rdev ) );
    memcpy( d += 8, &s.st_size, sizeof( s.st_size ) );
    memcpy( d += 8, &s.st_blksize, sizeof( s.st_blksize ) );
    memcpy( d += 8, &s.st_blocks, sizeof( s.st_blocks ) );
    memcpy( d += 8, &s.st_ino, sizeof( s.st_ino ) );

    memcpy( d += 8, &s.st_atim.tv_sec, sizeof( s.st_atim.tv_sec ) );
    memcpy( d += 4, &s.st_atim.tv_nsec, sizeof( s.st_atim.tv_nsec ) );
    memcpy( d += 8, &s.st_mtim.tv_sec, sizeof( s.st_mtim.tv_sec ) );
    memcpy( d += 4, &s.st_mtim.tv_nsec, sizeof( s.st_mtim.tv_nsec ) );
    memcpy( d += 8, &s.st_ctim.tv_sec, sizeof( s.st_ctim.tv_sec ) );
    memcpy( d += 4, &s.st_ctim.tv_nsec, sizeof( s.st_ctim.tv_nsec ) );

    return content;
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

    env.emplace_back( "vfs." + fmt( iCount ) + ".name", bstr( path.begin(), path.end() ) );
    env.emplace_back( "vfs." + fmt( iCount ) + ".stat", pStat );
    env.emplace_back( "vfs." + fmt( iCount ) + ".content", cont );
    limit(cont.size());

    if ( S_ISLNK( stat->st_mode ) ) {
        std::string symPath ( cont.begin(), cont.end() );
        if ( follow && !seen( symPath ) ) {
            see( symPath );
            auto ex = [&]( const std::string& item ) {
                return explore( follow, noPrefixChange, see, seen, count,
                    limit, env, item );
            };
            brick::fs::traverseDirectoryTree( symPath, ex, []( std::string ){}, ex );
        }
        return false;
    }
    return S_ISDIR( stat->st_mode );
}

} // namespace

void pruneBC( cc::Compile &driver )
{
    driver.prune( { "__sys_init", "main", "memmove", "memset",
                "memcpy", "llvm.global_ctors", "__lart_weakmem_buffer_size",
                "__md_get_function_meta", "__sys_env" } );
}

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
    for ( auto vfs : _vfs ) {
        int limit = vfs.sizeLimit;
        auto ex = [&](const std::string& item) {
            return explore( vfs.followSymlink,
                [&]( const std::string& s) { return changePathPrefix( s, vfs.capture, vfs.mount ); },
                [&]( const std::string& s ) { vfsCaptured.insert( s ); },
                [&]( const std::string& s ) { return visited( vfsCaptured, s); },
                [&]( ) { return i++; },
                [&]( int s ) { limit -= s; if ( limit < 0 ) die( "VFS capture limit reached"); },
                env,
                item );
        };

        if ( vfs.capture == vfs.mount ) {
            vfsCaptured.insert( vfs.capture );
        }
        brick::fs::traverseDirectoryTree( vfs.capture, ex, []( std::string ){ }, ex );
    }

    env.emplace_back( "divine.bcname", bstr( _file.begin(), _file.end() ) );


    try {
        _bc = std::make_shared< vm::BitCode >( _file );
    } catch ( vm::BCParseError &err ) /* probably not a bitcode file */
    {
        cc::Options ccopt;
        cc::Compile driver( ccopt );
        if ( !_std.empty() )
            driver.addFlags( { "-std=" + _std } );
        driver.setupFS( rt::each );
        driver.compileAndLink( _file, {} );
        pruneBC( driver );
        _bc = std::make_shared< vm::BitCode >(
            std::unique_ptr< llvm::Module >( driver.getLinked() ),
            driver.context() );
    }
    _bc->environment( env );
    _bc->autotrace( _autotrace );
    _bc->reduce( !_disableStaticReduction );
    _bc->lart( _lartPasses );
}

void Cc::run()
{
    if ( _files.empty() )
        die( "You must specify at least one source file." );
    if ( !_output.empty() && _drv.dont_link && _files.size() > 1 )
        die( "Cannot specify --dont-link -o with multiple input files." );

    cc::Compile driver( _drv );
    driver.setupFS( rt::each );

    for ( auto &i : _inc ) {
        driver.addDirectory( i );
        driver.addFlags( { "-I", i } );
    }
    for ( auto &i : _sysinc ) {
        driver.addDirectory( i );
        driver.addFlags( { "-isystem", i } );
    }

    for ( auto &x : _flags )
        x = "-"s + x; /* put back the leading - */

    if ( _drv.dont_link ) {
        for ( auto &x : _files ) {
            auto m = driver.compile( x, _flags );
            driver.writeToFile(
                _output.empty() ? outputName( x ) : _output,
                m.get() );
        }
    } else {
        for ( auto &x : _files )
            driver.compileAndLink( x, _flags );

        if ( !_drv.dont_link )
            pruneBC( driver );

        driver.writeToFile( _output.empty() ? outputName( _files.front() ) : _output );
    }
}

}
}
