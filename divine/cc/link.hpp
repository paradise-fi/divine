// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-
#pragma once

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
 
#include <memory>
#include <divine/cc/cc1.hpp>
#include <divine/cc/filetype.hpp>
#include <divine/cc/options.hpp>
#include <divine/vm/xg-code.hpp>

DIVINE_RELAX_WARNINGS
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Object/IRObjectFile.h>
#include <llvm/Support/MemoryBuffer.h>
DIVINE_UNRELAX_WARNINGS

#include <brick-string>
using namespace llvm;

namespace divine::cc
{
    using PairedFiles = std::vector< std::pair< std::string, std::string > >;

    namespace
    {
        bool whitelisted( llvm::Function &f )
        {
            using brick::string::startsWith;
            using vm::xg::hypercall;

            auto n = f.getName();
            return hypercall( &f ) != vm::lx::NotHypercall ||
                   startsWith( n, "__dios_" ) ||
                   startsWith( n, "_ZN6__dios" ) ||
                   startsWith( n, "_Unwind_" ) ||
                   n == "setjmp" || n == "longjmp";
        }

        bool whitelisted( llvm::GlobalVariable &gv )
        {
            return brick::string::startsWith( gv.getName(), "__md_" );
        }
    }

    void addSection( std::string filepath, std::string sectionName, const std::string &sectionData );
    std::vector< std::string > ld_args( cc::ParsedOpts& po, PairedFiles& objFiles );

    template < typename Driver, bool link_dios >
    std::unique_ptr< llvm::Module > link_bitcode( PairedFiles& files, cc::CC1& clang,
                                                  std::vector< std::string > libSearchPath )
    {
        auto drv = std::make_unique< Driver >( clang.context() );
        for( auto path : libSearchPath )
            drv->addDirectory( path );

        for ( auto file : files )
        {
            if ( !is_object_type( file.second ) )
            {
                if ( file.first != "lib" )
                    continue;
                else
                {
                    drv->linkLib( file.second, libSearchPath );
                    continue;
                }
            }

            ErrorOr< std::unique_ptr< MemoryBuffer > > buf = MemoryBuffer::getFile( file.second );
            if ( !buf ) throw cc::CompileError( "Error parsing file " + file.second + " into MemoryBuffer" );

            if ( is_type( file.second, FileType::Archive ) )
            {
                drv->linkArchive( std::move( buf.get() ) , clang.context() );
                continue;
            }

            auto bc = llvm::object::IRObjectFile::findBitcodeInMemBuffer( (*buf.get()).getMemBufferRef() );
            if ( !bc ) std::cerr << "No .llvmbc section found in file " << file.second << "." << std::endl;

            auto expected_m = llvm::parseBitcodeFile( bc.get(), *clang.context().get() );
            if ( !expected_m )
                std::cerr << "Error parsing bitcode." << std::endl;
            auto m = std::move( expected_m.get() );
            m->setTargetTriple( "x86_64-unknown-none-elf" );
            verifyModule( *m );
            drv->link( std::move( m ) );
        }

        if constexpr ( link_dios )
            drv->linkLibs( Driver::defaultDIVINELibs );

        auto m = drv->takeLinked();

        for ( auto& func : *m )
            if ( func.isDeclaration() && !whitelisted( func ) )
                throw cc::CompileError( "Symbol undefined (function): " + func.getName().str() );

        for ( auto& val : m->globals() )
            if ( auto G = dyn_cast< llvm::GlobalVariable >( &val ) )
                if ( !G->hasInitializer() && !whitelisted( *G ) )
                    throw cc::CompileError( "Symbol undefined (global variable): " + G->getName().str() );

        verifyModule( *m );
        return m;
    }
}
