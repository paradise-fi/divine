// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016 Petr Ročkai <code@fixp.eu>
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

DIVINE_RELAX_WARNINGS
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/IntrinsicInst.h>
DIVINE_UNRELAX_WARNINGS

#include <divine/vm/eval.hpp>
#include <divine/vm/print.hpp>
#include <divine/cc/runtime.hpp>
#include <brick-string>
#include <cxxabi.h>

namespace divine {
namespace vm {

enum class DNKind { Frame, Object };

static std::ostream &operator<<( std::ostream &o, DNKind dnk )
{
    switch ( dnk )
    {
        case DNKind::Frame: return o << "frame";
        case DNKind::Object: return o << "object";
    }
}

std::pair< std::string, int > fileline( const llvm::Instruction &insn );
std::string location( const llvm::Instruction &insn );

template< typename Context >
struct DebugNode
{
    using Heap = typename Context::Heap;
    using Program = typename Context::Program;
    using Eval = Eval< Program, ConstContext< Program, Heap >, value::Void >;
    using Snapshot = typename Heap::Snapshot;

    ConstContext< Program, Heap > _ctx;

    GenericPointer _address;
    Snapshot _snapshot;
    DNKind _kind;

    /* applies only to Objects */
    llvm::Type *_type;
    llvm::DIType *_di_type;

    using PointerV = value::Pointer;

    DebugNode( Context ctx, Snapshot s, GenericPointer l, DNKind k, llvm::Type *t, llvm::DIType *dit )
        : _ctx( ctx.program(), ctx.heap() ), _address( l ), _snapshot( s ), _kind( k ),
          _type( t ), _di_type( dit )
    {
        _ctx.heap().restore( s );
        _ctx.globals( ctx.globals() );
        _ctx.constants( ctx.constants() );
        if ( k == DNKind::Frame )
            _ctx.frame( l );
    }

    DebugNode( ConstContext< Program, Heap > ctx, Snapshot s,
               GenericPointer l, DNKind k, llvm::Type *t, llvm::DIType *dit )
        : _ctx( ctx ), _address( l ), _snapshot( s ), _kind( k ), _type( t ), _di_type( dit )
    {
        if ( k == DNKind::Frame )
            _ctx.frame( l );
    }

    DebugNode( const DebugNode &o ) = default;

    void relocate( typename Heap::Snapshot s )
    {
        _ctx.heap().restore( s );
    }

    DNKind kind() { return _kind; }
    GenericPointer address() { return _address; }
    Snapshot snapshot() { return _snapshot; }

    template< typename B >
    void hexbyte( std::ostream &o, int &col, int index, B byte )
    {
        o << std::setw( 2 ) << std::setfill( '0' ) << std::setbase( 16 ) << +byte;
        col += 2;
        if ( index % 2 == 1 )
            ++col, o << " ";
    }

    template< typename B >
    void ascbyte( std::ostream &o, int &col, B byte )
    {
        std::stringstream os;
        os << byte;
        char b = os.str()[ 0 ];
        o << ( std::isprint( b ) ? b : '~' );
        ++ col;
    }

    bool valid()
    {
        if ( _address.null() )
            return false;
        if ( _address.type() == PointerType::Heap && !_ctx.heap().valid( _address ) )
            return false;
        return true;
    }

    template< typename Y >
    void attributes( Y yield )
    {
        Eval eval( _ctx.program(), _ctx );
        Program &program = _ctx.program();

        yield( "address", brick::string::fmt( PointerV( _address ) ) );

        if ( _di_type )
            yield( "type", _di_type->getName().str() );

        if ( !valid() )
            return;

        std::stringstream raw;

        if ( _address.type() == PointerType::Heap )
            ASSERT_EQ( _address.offset(), 0 );
        int sz = eval.ptr2sz( _address );
        auto hloc = eval.ptr2h( _address );
        auto bytes = _ctx.heap().unsafe_bytes( hloc, hloc.offset(), sz );
        auto types = _ctx.heap().type( hloc, hloc.offset(), sz );
        auto defined = _ctx.heap().defined( hloc, hloc.offset(), sz );

        if ( _type && ( _type->isIntegerTy() || _type->isFloatingPointTy() ))
            eval.template type_dispatch< Any >(
                _type->getPrimitiveSizeInBits(),
                _type->isFloatingPointTy() ? Program::Slot::Float : Program::Slot::Integer,
                [&]( auto v )
                {
                    yield( "value", brick::string::fmt( v.get( _address ) ) );
                } );

        for ( int c = 0; c < ( sz / 12 ) + ( sz % 12 ? 1 : 0 ); ++c )
        {
            int col = 0;
            for ( int i = c * 12; i < std::min( (c + 1) * 12, sz ); ++i )
                hexbyte( raw, col, i, bytes[ i ] );
            print::pad( raw, col, 30 ); raw << "| ";
            for ( int i = c * 12; i < std::min( (c + 1) * 12, sz ); ++i )
                hexbyte( raw, col, i, defined[ i ] );
            print::pad( raw, col, 60 ); raw << "| ";
            for ( int i = c * 12; i < std::min( (c + 1) * 12, sz ); ++i )
                ascbyte( raw, col, bytes[ i ] );
            print::pad( raw, col, 72 ); raw << " | ";
            for ( int i = c * 12; i < std::min( (c + 1) * 12, sz ); ++i )
                ascbyte( raw, col, types[ i ] );
            print::pad( raw, col, 84 );
            if ( c + 1 < ( sz / 12 ) + ( sz % 12 ? 1 : 0 ) )
                raw << std::endl;
        }

        yield( "_raw", raw.str() );

        if ( _address.type() == PointerType::Const )
        {
            ConstPointer pp = _address;
            yield( "slot", brick::string::fmt( program._constants[ pp.object() ] ) );
        }

        if ( _address.type() == PointerType::Global )
        {
            GlobalPointer pp = _address;
            yield( "slot", brick::string::fmt( program._globals[ pp.object() ] ) );
        }

        if ( _kind == DNKind::Frame )
        {
            if ( pc().null() )
                return;
            auto *insn = &program.instruction( pc() );
            if ( insn->op )
                yield( "instruction", print::instruction( eval ) );
            if ( !insn->op )
                insn = &program.instruction( pc() + 1 );
            ASSERT( insn->op );
            auto op = llvm::cast< llvm::Instruction >( insn->op );
            yield( "location", location( *op ) );

            auto sym = op->getParent()->getParent()->getName().str();
            yield( "symbol", demangle( sym ) );
        }
    }


    std::string demangle( std::string mangled )
    {
        int stat;
        auto x = abi::__cxa_demangle( mangled.c_str(), nullptr, nullptr, &stat );
        auto ret = stat == 0 && x ? std::string( x ) : mangled;
        std::free( x );
        return ret;
    }

    CodePointer pc()
    {
        ASSERT_EQ( kind(), DNKind::Frame );
        PointerV pc;
        _ctx.heap().read( _address, pc );
        return pc.cooked();
    }

    llvm::DISubprogram *subprogram()
    {
        ASSERT_EQ( kind(), DNKind::Frame );
        auto F = _ctx.program().llvmfunction( pc() );
        return llvm::getDISubprogram( F );
    }

    void bitcode( std::ostream &out )
    {
        ASSERT_EQ( _kind, DNKind::Frame );
        Eval eval( _ctx.program(), _ctx );
        CodePointer iter = pc();
        iter.instruction( 0 );
        auto &instructions = _ctx.program().function( iter ).instructions;
        for ( auto &i : instructions )
        {
            eval._instruction = &i;
            out << ( iter == pc() ? ">>" : "  " );
            if ( i.op )
                out << print::instruction( eval, 2 ) << std::endl;
            else
            {
                auto iop = llvm::cast< llvm::Instruction >( instructions[ iter.instruction() + 1 ].op );
                auto name = iop->getParent()->getName().str();
                out << "label %"
                    << ( name.empty() ? brick::string::fmt( iter.instruction() ) : name )
                    << ":" << std::endl;
            }
            iter = iter + 1;
        }
    }

    void source( std::ostream &out )
    {
        ASSERT_EQ( _kind, DNKind::Frame );
        brick::string::Splitter split( "\n", REG_EXTENDED );
        auto di = subprogram();
        auto src = cc::runtime::source( di->getFilename() );
        if ( src.empty() )
            src = brick::fs::readFile( di->getFilename() );
        /* TODO: also deal with sources outside of runtime! */
        auto line = split.begin( src );
        unsigned lineno = 1, active = 0;
        while ( lineno < di->getLine() )
            ++ line, ++ lineno;
        unsigned endline = lineno;

        /* figure out the source code span the function covers; painfully */
        for ( auto &i : _ctx.program().function( pc() ).instructions )
        {
            if ( !i.op )
                continue;
            auto op = llvm::cast< llvm::Instruction >( i.op );
            auto dl = op->getDebugLoc().get();
            if ( !dl )
                continue;
            dl = dl->getInlinedAt() ?: dl;
            if ( _ctx.program().instruction( pc() ).op == i.op )
                active = dl->getLine();
            endline = std::max( endline, dl->getLine() );
        }

        /* print it */
        while ( line != split.end() && lineno <= endline )
        {
            out << (lineno == active ? ">>" : "  ");
            out << std::setw( 5 ) << lineno++ << " " << *line++ << std::endl;
        }
        brick::string::ERegexp endbrace( "^[ \t]*}" );
        if ( endbrace.match( *line ) )
            out << "  " << std::setw( 5 ) << lineno++ << " " << *line++ << std::endl;
    }

    template< typename Y >
    void related( Y yield )
    {
        if ( !valid() )
            return;

        Eval eval( _ctx.program(), _ctx );

        PointerV ptr;
        auto hloc = eval.ptr2h( _address );
        int sz = eval.ptr2sz( _address ), hoff = hloc.offset();

        int i = 0;
        for ( auto ptroff : _ctx.heap().pointers( hloc, hloc.offset(), sz ) )
        {
            hloc.offset( hoff + ptroff->offset() );
            _ctx.heap().read( hloc, ptr );
            auto pp = ptr.cooked();
            if ( pp.type() == PointerType::Code )
                continue;
            pp.offset( 0 );
            yield( "_ptr_" + brick::string::fmt( i++ ),
                   DebugNode( _ctx, _snapshot, pp, DNKind::Object, nullptr, nullptr ) );
        }

        if ( _type && _di_type && _type->isStructTy() )
        {
            auto CT = llvm::cast< llvm::DICompositeType >( _di_type );
            auto ST = llvm::cast< llvm::StructType >( _type );
            auto STE = ST->element_begin();
            auto SLO = _ctx.program().TD.getStructLayout( ST );
            int idx = 0;
            for ( auto subtype : CT->getElements() )
                if ( auto CTE = llvm::dyn_cast< llvm::DIDerivedType >( subtype ) )
                {
                    yield( "+" + CTE->getName().str(),
                           DebugNode( _ctx, _snapshot, _address + SLO->getElementOffset( idx ),
                                      DNKind::Object, *STE, CTE ) );
                    STE ++;
                    idx ++;
                }
        }

        if ( _kind == DNKind::Frame )
        {
            PointerV fr = _address;
            _ctx.heap().skip( fr, PointerBytes );
            _ctx.heap().read( fr.cooked(), fr );
            if ( !fr.cooked().null() )
                yield( "parent", DebugNode( _ctx, _snapshot, fr.cooked(), DNKind::Frame, nullptr, nullptr ) );

            auto *insn = &_ctx.program().instruction( pc() );
            if ( !insn->op )
                insn = &_ctx.program().instruction( pc() + 1 );
            auto op = llvm::cast< llvm::Instruction >( insn->op );
            auto F = op->getParent()->getParent();

            for ( auto &BB : *F )
                for ( auto &I : BB )
                    if ( auto DDI = llvm::dyn_cast< llvm::DbgDeclareInst >( &I ) )
                    {
                        auto divar = DDI->getVariable();
                        auto ditype = divar->getType().resolve( _ctx.program().ditypemap );
                        auto var = DDI->getAddress();
                        auto &vmap = _ctx.program().valuemap;
                        if ( vmap.find( var ) == vmap.end() )
                            continue;

                        PointerV ptr;
                        _ctx.heap().read( eval.s2ptr( _ctx.program().valuemap[ var ].slot ), ptr );

                        yield( std::string( "+" ) + divar->getName().str(),
                               DebugNode( _ctx, _snapshot, ptr.cooked(), DNKind::Object,
                                          var->getType()->getPointerElementType(), ditype ) );
                    }
        }
    }

    void dump( std::ostream &o );
    void dot( std::ostream &o );
};

}
}
