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
#include <brick-string>

DIVINE_RELAX_WARNINGS
#include <brick-llvm>
DIVINE_UNRELAX_WARNINGS

namespace divine {
namespace ui {

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
    vm::BitCode::Env env;
    using bstr = std::vector< uint8_t >;
    int i = 0;
    for ( auto s : _env )
        env.emplace_back( "env." + brick::string::fmt( i++ ), bstr( s.begin(), s.end() ) );
    i = 0;
    for ( auto o : _useropts )
        env.emplace_back( "arg." + brick::string::fmt( i++ ), bstr( o.begin(), o.end() ) );
    env.emplace_back( "divine.bcname", bstr( _file.begin(), _file.end() ) );

    try {
        _bc = std::make_shared< vm::BitCode >( _file, env, _autotrace );
    } catch ( vm::BCParseError &err ) /* probably not a bitcode file */
    {
        cc::Options ccopt;
        ccopt.precompiled = "libdivinert.bc";
        cc::Compile driver( ccopt );
        driver.compileAndLink( _file, {} );
        pruneBC( driver );
        _bc = std::make_shared< vm::BitCode >(
            std::unique_ptr< llvm::Module >( driver.getLinked() ),
            driver.context(), env, _autotrace );
    }
}

void Cc::run()
{
    if ( !_drv.libs_only && _files.empty() )
        die( "Either a file to build or --libraries-only is required." );
    if ( _drv.libs_only && !_files.empty() )
        die( "Cannot specify both --libraries-only and files to compile." );
    if ( !_output.empty() && _drv.dont_link && _files.size() > 1 )
        die( "Cannot specify --dont-link -o with multiple input files." );

    if ( _output.empty() && _drv.libs_only )
        _output = "libdivinert.bc";

    cc::Compile driver( _drv );

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

        if ( !_drv.dont_link && !_drv.libs_only )
            pruneBC( driver );

        driver.writeToFile( _output.empty() ? outputName( _files.front() ) : _output );
    }
}

void Run::run() { vm::Run( _bc ).run(); }

}
}
