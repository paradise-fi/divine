// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2017 Petr Ročkai <code@fixp.eu>
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

#pragma once

#include <divine/vm/pointer.hpp>
#include <divine/vm/program.hpp>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/DataLayout.h>
DIVINE_UNRELAX_WARNINGS

namespace divine::vm::dbg
{

std::pair< llvm::StringRef, int > fileline( const llvm::Instruction &insn );
std::string location( const llvm::Instruction &insn );
std::string location( std::pair< llvm::StringRef, int > );

using ProcInfo = std::vector< std::pair< std::pair< int, int >, int > >;

struct Info
{
    llvm::Function *function( CodePointer pc )
    {
        return _funmap[ pc.function() ];
    }

    auto find( llvm::Instruction *I, CodePointer pc )
        -> std::pair< llvm::Instruction *, CodePointer >
    {
        llvm::Function *F = I ? I->getParent()->getParent() : function( pc );
        CodePointer pcf( pc.function() ? pc.function() : _program._addr.code( F ).function(), 0 );
        ASSERT( F );
        ASSERT( pcf.function() );

        for ( auto &BB : *F )
        {
            ASSERT_EQ( _program.instruction( pcf ).opcode, lx::OpBB );
            pcf = pcf + 1;
            for ( auto it = BB.begin(); it != BB.end(); it = std::next( it ) )
            {
                if ( ( I && I == &*it ) || ( pc.function() && pc == pcf ) )
                    return std::make_pair( &*it, pcf );
                pcf = pcf + 1;
            }
        }
        UNREACHABLE( "dbg::Info::find() failed" );
    }

    llvm::DataLayout &layout() { return _layout; }
    llvm::DITypeIdentifierMap &typemap() { return _typemap; }

    llvm::DITypeIdentifierMap _typemap;
    llvm::DataLayout _layout;
    llvm::Module *_module;
    Program &_program;
    std::map< int, llvm::Function * > _funmap;

    Info( llvm::Module *m, Program &p ) : _layout( m ), _module( m ), _program( p )
    {
        CodePointer pc( 0, 0 );
        for ( auto &f : *m )
        {
            if ( f.isDeclaration() )
                continue;
            pc.function( pc.function() + 1 );
            _funmap[ pc.function() ] = &f;
        }

        auto dbg_cu = m->getNamedMetadata( "llvm.dbg.cu" );
        if ( dbg_cu )
            _typemap = llvm::generateDITypeIdentifierMap( dbg_cu );
    }
};

}
