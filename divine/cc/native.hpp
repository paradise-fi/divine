// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-
#pragma once

/*
 * (c) 2019 Zuzana Baranová <xbaranov@fi.muni.cz>
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
#include <divine/cc/options.hpp>

DIVINE_RELAX_WARNINGS
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Object/IRObjectFile.h>
DIVINE_UNRELAX_WARNINGS

namespace divine::cc
{
    /* PairedFiles is used for mapping IN => OUT filenames at compilation,
     * or checking whether to compile a given file,
     * i.e. file.c => compiled to file.o; file.o => will not be compiled,
     * similarly for archives; the naming can further be affected by the -o
     * option.
     */
    using PairedFiles = std::vector< std::pair< std::string, std::string > >;

    // The layer below divcc tool but above the driver and CC1.
    // It is responsible for holding the parsed command line arguments,
    // providing high-level access to preprocessing, compiling and linking.
    struct Native
    {
        // 'args' contains command line arguments for this invocation of the compiler,
        // which are handed to the parser and processed into _po.
        Native( const std::vector< std::string >& args );
        
        template< typename Driver >
        void link_bitcode_file( std::pair< std::string, std::string > file, Driver &drv )
        {
            if ( !is_object_type( file.second ) )
            {
                if ( file.first != "lib" )
                    return;
                try
                {
                    drv->linkLib( file.second, _po.libSearchPath );
                }
                catch( std::exception )
                {
                    if ( _missing_bc_fatal )
                        throw;
                    else
                        std::cerr << "Warning: bitcode not found in lib: " << file.second << std::endl;
                }
                return;
            }
            if ( is_type( file.second, FileType::Archive ) || is_type( file.second, FileType::Shared ) )
            {
                drv->linkLib( file.second, { "." }, is_type( file.second, FileType::Shared ) );
                return;
            }
            else
            {
                auto obj = drv->load_object( file.second );
                obj->setTargetTriple( "x86_64-unknown-none-elf" );
                drv->link( std::move( obj ) );
            }
            return;
        }

        template< typename Driver >
        std::unique_ptr< llvm::Module > do_link_bitcode()
        {
            auto drv = std::make_unique< Driver >( _clang.context() );
            for( auto path : _po.libSearchPath )
                drv->addDirectory( path );

            for ( auto file : _files )
                link_bitcode_file( file, drv );

            auto m = drv->takeLinked();
            verifyModule( *m );
            return m;
        }

        int compile_files();
        void init_ld_args();
        virtual void link();
        void preprocess_only();
        int run();
        virtual std::unique_ptr< llvm::Module > link_bitcode();
        void construct_paired_files();
        void print_info( std::string_view version );
        void print_info( std::vector< const char *>& args );
        void set_cxx( bool cxx ) { _cxx = cxx; }
        virtual const char* tool_name() { return "divcc"; }

        cc::ParsedOpts _po;
        PairedFiles _files;
        std::vector< std::string > _ld_args;
        cc::CC1 _clang;
        bool _cxx;
        bool _missing_bc_fatal = false;  // abort at a file/lib without bitcode?

        ~Native();
    };
}
