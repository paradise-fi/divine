// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2017 Jan Horáček <me@apophis.cz>
 * (c) 2017-2019 Zuzana Baranová <xbaranov@fi.muni.cz>
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

#include <divine/cc/cc1.hpp>
#include <divine/cc/codegen.hpp>
#include <divine/cc/filetype.hpp>
#include <divine/cc/link.hpp>
#include <divine/cc/native.hpp>
#include <divine/cc/options.hpp>
#include <divine/rt/dios-cc.hpp>
#include <divine/rt/runtime.hpp>
#include <divine/ui/version.hpp>

DIVINE_RELAX_WARNINGS
#include "llvm/Target/TargetMachine.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Object/IRObjectFile.h"
#include "llvm-c/Target.h"

DIVINE_UNRELAX_WARNINGS

#include <brick-llvm>
#include <brick-string>
#include <brick-proc>

using namespace divine;
using namespace llvm;
using namespace brick::types;

using FileType = cc::FileType;

auto link_dios_native( std::vector< std::string > &args, bool cxx )
{
    using namespace brick::fs;
    TempDir tmpdir( ".divcc.XXXXXX", AutoDelete::Yes, UseSystemTemp::Yes );
    auto hostlib = tmpdir.path + "/libdios-host.a",
         cxxlib  = tmpdir.path + "/libc++.a",
         cxxabi  = tmpdir.path + "/libc++abi.a";

    if ( cxx )
    {
        args.push_back( "--driver-mode=g++" );
        args.push_back( "-stdlib=libc++" );
        args.push_back( "-L" + tmpdir.path );
        writeFile( cxxlib, rt::libcxx() );
        writeFile( cxxabi, rt::libcxxabi() );
    }

    writeFile( hostlib, rt::dios_host() );
    args.push_back( hostlib );

    return tmpdir;
}

/* usage: same as gcc */
int main( int argc, char **argv )
{
    try {
        cc::Native nativeCC;
        cc::CC1& clang = nativeCC._clang;
        clang.allowIncludePath( "/" );
        divine::rt::each( [&]( auto path, auto c ) { clang.mapVirtualFile( path, c ); } );

        std::vector< std::string > opts;
        std::copy( argv + 1, argv + argc, std::back_inserter( opts ) );
        nativeCC._po = cc::parseOpts( opts );
        auto& po = nativeCC._po;
        auto& pairedFiles = nativeCC._files;

        using namespace brick::fs;
        using divine::rt::includeDir;

        auto driver = std::make_unique< cc::Driver >( clang.context() );

        po.opts.insert( po.opts.end(),
                        driver->commonFlags.begin(),
                        driver->commonFlags.end() );

        po.opts.insert( po.opts.end(), {
                        "-isystem", joinPath( includeDir, "libcxx/include" )
                      , "-isystem", joinPath( includeDir, "libcxxabi/include" )
                      , "-isystem", joinPath( includeDir, "libunwind/include" )
                      , "-isystem", includeDir } );

        // count files, i.e. not libraries
        auto num_files = std::count_if( po.files.begin(), po.files.end(),
                                        []( cc::FileEntry f ){ return f.is< cc::File >(); } );

        if ( num_files > 1 && po.outputFile != ""
             && ( po.preprocessOnly || po.toObjectOnly ) )
        {
            std::cerr << "Cannot specify -o with multiple files" << std::endl;
            return 1;
        }

        using namespace clang;

        if ( po.hasHelp || po.hasVersion )
        {
            if ( po.hasVersion )
                std::cout << "divine version: " << ui::version() << "\n";
            cc::ClangDriver drv;
            delete drv.BuildCompilation( { "divcc", po.hasHelp ? "--help" : "--version" } );
            return 0;
        }

        if ( po.preprocessOnly )
        {
            for ( auto srcFile : po.files )
            {
                std::string ifn = srcFile.get< cc::File >().name;
                if ( cc::is_object_type( ifn ) )
                    continue;
                std::cout << clang.preprocess( ifn, po.opts );
            }
            return 0;
        }

        for ( auto srcFile : po.files )
        {
            if ( srcFile.is< cc::File >() )
            {
                std::string ifn = srcFile.get< cc::File >().name;
                std::string ofn = dropExtension( ifn );
                ofn = splitFileName( ofn ).second;
                if ( po.outputFile != "" && po.toObjectOnly )
                    ofn = po.outputFile;
                else
                {
                    if ( !po.toObjectOnly )
                        ofn += ".divcc.temp";
                    ofn += ".o";
                }

                if ( cc::is_object_type( ifn ) )
                    ofn = ifn;
                pairedFiles.emplace_back( ifn, ofn );
            }
            else
            {
                assert( srcFile.is< cc::Lib >() );
                pairedFiles.emplace_back( "lib", srcFile.get< cc::Lib >().name );
            }
        }

        if ( po.toObjectOnly )
            return nativeCC.compile_files();
        else
        {
            nativeCC.compile_files();
            nativeCC.init_ld_args();
            auto tmpdir = link_dios_native( nativeCC._ld_args, brick::string::endsWith( argv[0], "divc++" ) );
            nativeCC.link();

            std::unique_ptr< llvm::Module > mod = cc::link_bitcode< rt::DiosCC, true >( pairedFiles, clang, po.libSearchPath );
            std::string file_out = po.outputFile != "" ? po.outputFile : "a.out";

            cc::add_section( file_out, cc::llvm_section_name, clang.serializeModule( *mod ) );
            return 0;
        }

    } catch ( cc::CompileError &err ) {
        std::cerr << err.what() << std::endl;
        return 1;
    } catch ( std::runtime_error &err ) {
        std::cerr << "compilation failed: " << err.what() << std::endl;
        return 2;
    }
}
