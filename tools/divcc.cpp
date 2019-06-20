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
#include "lld/Common/Driver.h"
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

int link( cc::ParsedOpts& po, cc::CC1& clang, cc::PairedFiles& objFiles, bool cxx = false )
{
    auto drv = std::make_unique< cc::Driver >( clang.context() );
    std::vector< const char * > ld_args_c;

    std::vector< std::string > args = cc::ld_args( po, objFiles );

    auto tmpdir = link_dios_native( args, cxx );
    ld_args_c.reserve( args.size() );
    for ( size_t i = 0; i < args.size(); ++i )
        ld_args_c.push_back( args[i].c_str() );

    auto ld_job = drv->getJobs( ld_args_c ).back();
    if ( po.use_system_ld )
    {
        ld_job.args.insert( ld_job.args.begin(), ld_job.name );
        auto r = brick::proc::spawnAndWait( brick::proc::CaptureStderr, ld_job.args );
        if ( !r )
            throw cc::CompileError( "failed to link, ld exited with " + to_string( r ) );
    }
    else
    {
        ld_job.args.insert( ld_job.args.begin(), "divcc" );
        std::vector< const char * > lld_job_c;
        lld_job_c.reserve( ld_job.args.size() );
        for ( size_t i = 0; i < ld_job.args.size(); ++i )
            lld_job_c.push_back( ld_job.args[i].c_str() );

        bool linked = lld::elf::link( lld_job_c, false );
        if ( !linked )
            throw cc::CompileError( "lld failed, not linked" );
    }

    std::unique_ptr< llvm::Module > mod = cc::link_bitcode< rt::DiosCC, true >( objFiles, clang, po.libSearchPath );
    std::string file_out = po.outputFile != "" ? po.outputFile : "a.out";

    cc::addSection( file_out, cc::llvm_section_name, clang.serializeModule( *mod ) );

    for ( auto file : objFiles )
    {
        if ( cc::is_object_type( file.first ) )
            continue;
        std::string ofn = file.second;
        unlink( ofn.c_str() );
    }

    return 0;
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

        auto drv = std::make_unique< cc::Driver >( clang.context() );

        po.opts.insert( po.opts.end(),
                        drv->commonFlags.begin(),
                        drv->commonFlags.end() );

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
            return nativeCC.compileFiles();
        else
        {
            nativeCC.compileFiles();
            return link( po, clang, pairedFiles, brick::string::endsWith( argv[0], "divc++" ) );
        }

    } catch ( cc::CompileError &err ) {
        std::cerr << err.what() << std::endl;
        return 1;
    } catch ( std::runtime_error &err ) {
        std::cerr << "compilation failed: " << err.what() << std::endl;
        return 2;
    }
}
