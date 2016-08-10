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
DIVINE_UNRELAX_WARNINGS

#include <divine/vm/eval.hpp>
#include <brick-string>

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

static std::string fileline( const llvm::Instruction &insn )
{
    auto loc = insn.getDebugLoc().get();
    if ( loc && loc->getNumOperands() )
        return loc->getFilename().str() + std::string( ":" ) +
            brick::string::fmt( loc->getLine() );
    return "(unknown location)";
}

template< typename Context >
struct DebugNode
{
    Context *_ctx;
    GenericPointer _address;
    DNKind _kind;
    llvm::Type *_type; /* applies only to Objects */

    using HeapPointer = typename Context::Heap::Pointer;
    using PointerV = value::Pointer< HeapPointer >;

    DebugNode( Context &c, GenericPointer l, DNKind k, llvm::Type *t )
        : _ctx( &c ), _address( l ), _kind( k ), _type( t )
    {}

    DebugNode( const DebugNode &o ) = default;
    DebugNode() : _ctx( nullptr ), _address( vm::nullPointer() ) {}

    DNKind kind() { return _kind; }

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

    void pad( std::ostream &o, int &col, int target )
    {
        while ( col < target )
            ++col, o << " ";
    }

    template< typename Y >
    void attributes( Y yield )
    {
        Eval< Program, Context, value::Void > eval( _ctx->program(), *_ctx );
        Program &program = _ctx->program();

        yield( "address", brick::string::fmt( PointerV( _address ) ) );

        if ( _address == nullPointer() )
            return;

        std::stringstream raw;

        int sz = eval.ptr2sz( _address );
        auto hloc = eval.ptr2h( _address );
        auto bytes = _ctx->heap().unsafe_bytes( hloc, hloc.offset(), sz );
        auto types = _ctx->heap().type( hloc, hloc.offset(), sz );
        auto defined = _ctx->heap().defined( hloc, hloc.offset(), sz );

        for ( int c = 0; c < ( sz / 12 ) + ( sz % 12 ? 1 : 0 ); ++c )
        {
            int col = 0;
            for ( int i = c * 12; i < std::min( (c + 1) * 12, sz ); ++i )
                hexbyte( raw, col, i, bytes[ i ] );
            pad( raw, col, 30 ); raw << "| ";
            for ( int i = c * 12; i < std::min( (c + 1) * 12, sz ); ++i )
                hexbyte( raw, col, i, defined[ i ] );
            pad( raw, col, 60 ); raw << "| ";
            for ( int i = c * 12; i < std::min( (c + 1) * 12, sz ); ++i )
                ascbyte( raw, col, bytes[ i ] );
            pad( raw, col, 72 ); raw << " | ";
            for ( int i = c * 12; i < std::min( (c + 1) * 12, sz ); ++i )
                ascbyte( raw, col, types[ i ] );
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
            PointerV pc;
            _ctx->heap().read( hloc, pc );
            auto op = program.instruction( pc.cooked() ).op;
            if ( op )
            {
                auto &insn = *llvm::cast< llvm::Instruction >( op );
                yield( "location", fileline( insn ) );
                yield( "symbol", insn.getParent()->getParent()->getName().str() );
            }
        }
    }

    template< typename Y >
    void related( Y yield )
    {
        if ( _address == nullPointer() )
            return;

        Eval< Program, Context, value::Void > eval( _ctx->program(), *_ctx );

        PointerV ptr;
        auto hloc = eval.ptr2h( _address );
        int sz = eval.ptr2sz( _address ), hoff = hloc.offset();

        int i = 0;
        for ( auto ptroff : _ctx->heap().pointers( hloc, hloc.offset(), sz ) )
        {
            hloc.offset( hoff + ptroff->offset() );
            _ctx->heap().read( hloc, ptr );
            yield( "_ptr_" + brick::string::fmt( i++ ),
                    DebugNode( *_ctx, ptr.cooked(), DNKind::Object, nullptr ) );
        }

        if ( _kind == DNKind::Frame )
        {
            PointerV fr = _address;
            _ctx->heap().skip( fr, PointerBytes );
            _ctx->heap().read( fr.cooked(), fr );
            if ( fr.cooked() != nullPointer() )
                yield( "parent", DebugNode( *_ctx, fr.cooked(), DNKind::Frame, nullptr ) );
        }
    }

    void dump( std::ostream &o );
    void dot( std::ostream &o );
    void backtrace( typename Context::Heap::Pointer top, std::ostream &o );
};

}
}
