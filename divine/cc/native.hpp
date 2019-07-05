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

    struct Native
    {
        Native( const std::vector< std::string >& args );

        template< typename Driver >
        std::unique_ptr< llvm::Module > do_link_bitcode()
        {
            auto drv = std::make_unique< Driver >( _clang.context() );
            for( auto path : _po.libSearchPath )
                drv->addDirectory( path );

            for ( auto file : _files )
            {
                if ( !is_object_type( file.second ) )
                {
                    if ( file.first != "lib" )
                        continue;
                    else
                    {
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
                        continue;
                    }
                }

                llvm::ErrorOr< std::unique_ptr< llvm::MemoryBuffer > > buf = llvm::MemoryBuffer::getFile( file.second );
                if ( !buf ) throw cc::CompileError( "Error parsing file " + file.second + " into MemoryBuffer" );

                if ( is_type( file.second, FileType::Archive ) )
                {
                    drv->linkArchive( std::move( buf.get() ) , _clang.context() );
                    continue;
                }

                auto bc = llvm::object::IRObjectFile::findBitcodeInMemBuffer( (*buf.get()).getMemBufferRef() );
                if ( !bc ) std::cerr << "No .llvmbc section found in file " << file.second << "." << std::endl;

                auto expected_m = llvm::parseBitcodeFile( bc.get(), *_clang.context().get() );
                if ( !expected_m )
                    std::cerr << "Error parsing bitcode." << std::endl;
                auto m = std::move( expected_m.get() );
                m->setTargetTriple( "x86_64-unknown-none-elf" );
                verifyModule( *m );
                drv->link( std::move( m ) );
            }

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
        void set_cxx( bool cxx ) { _cxx = cxx; }

        cc::ParsedOpts _po;
        PairedFiles _files;
        std::vector< std::string > _ld_args;
        cc::CC1 _clang;
        bool _cxx;
        bool _missing_bc_fatal = false;  // abort at a file/lib without bitcode?

        ~Native();
    };
}
