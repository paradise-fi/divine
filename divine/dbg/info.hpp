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

namespace divine::dbg
{

enum class Component
{
    Kernel      = 0b000001,
    DiOS        = 0b000010, /* non-kernel dios components */
    LibC        = 0b000100,
    LibCxx      = 0b001000,
    LibRst      = 0b010000,
    Program     = 0b100000
};

using Components = brick::types::StrongEnumFlags< Component >;

std::pair< llvm::StringRef, int > fileline( const llvm::Instruction &insn );
std::string location( const llvm::Instruction &insn );
std::string location( std::pair< llvm::StringRef, int > );

using ProcInfo = std::vector< std::pair< std::pair< int, int >, int > >;

struct Info
{
    llvm::Function *function( vm::CodePointer pc )
    {
        return _funmap[ pc.function() ];
    }

    std::pair< llvm::StringRef, int > fileline( vm::CodePointer pc );
    bool in_component( vm::CodePointer pc, Components comp );

    auto find( llvm::Instruction *I, vm::CodePointer pc )
        -> std::pair< llvm::Instruction *, vm::CodePointer >
    {
        llvm::Function *F = I ? I->getParent()->getParent() : function( pc );

        vm::CodePointer pcf = pc;
        if ( !pc.function() )
            pcf = _program._addr.code( F );
        pcf = _program.entry( pcf );

        ASSERT( F );
        ASSERT( pcf.function() );

        /* no LLVM instruction corresponds to OpBB */
        if ( pc.function() && ( _program.instruction( pc ).opcode == vm::lx::OpBB ||
                                pc.instruction() < pcf.instruction() ) )
            return std::make_pair( nullptr, pc );

        for ( auto &BB : *F )
        {
            ASSERT_EQ( _program.instruction( pcf ).opcode, vm::lx::OpBB );
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

    template < typename F >
    void initPretty( F getName ) {
        if ( _typenamemap.size() == _prettyNames.size() )
            return;
        for ( const auto& x : _typenamemap )
            _prettyNames.insert( { getName( x.first ), x.second } );
    }

    std::string makePretty( std::string name ) {
        for ( const auto& pattern : _prettyNames ) {
            size_t start = 0;
            while( true ) {
                start = name.find( pattern.first, start );
                if ( start == std::string::npos )
                    break;
                name.replace( start, pattern.first.size(), pattern.second );
                start += pattern.second.size();
            }
        }
        return name;
    }

    struct StringLenCmp {
        bool operator()( const std::string& a, const std::string& b ) const {
            if ( a.size() == b.size() )
                return a < b;
            return a.size() > b.size();
        }
    };

    llvm::DataLayout &layout() { return _layout; }
    llvm::DITypeIdentifierMap &typemap() { return _typemap; }

    llvm::DITypeIdentifierMap _typemap;
    llvm::DataLayout _layout;
    llvm::Module *_module;
    vm::Program &_program;
    std::map< int, llvm::Function * > _funmap;
    std::map< llvm::DIType *, std::string > _typenamemap;
    std::map< std::string, std::string, StringLenCmp > _prettyNames;

    Info( llvm::Module *m, vm::Program &p ) : _layout( m ), _module( m ), _program( p )
    {
        vm::CodePointer pc( 0, 0 );
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
