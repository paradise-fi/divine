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

#include <divine/vm/pointer.hpp>
#include <divine/vm/program.hpp>
#include <divine/cc/runtime.hpp>

#include <cxxabi.h>
#include <brick-fs>

namespace divine {
namespace vm {

namespace print {

std::string opcode( int );

static void pad( std::ostream &o, int &col, int target )
{
    while ( col < target )
        ++col, o << " ";
}

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

template< typename Eval >
static std::string instruction( Eval &eval, int padding = 0 )
{
    std::stringstream out;
    auto &insn = eval.instruction();
    ASSERT( insn.op );
    llvm::Instruction *I = llvm::cast< llvm::Instruction >( insn.op );
    std::string iname = I->getName();
    int anonid = eval.program().pcmap[ I ].instruction();

    if ( iname.empty() )
        iname = brick::string::fmt( anonid );
    if ( insn.result().type != Program::Slot::Void )
        out << "  %" << iname << " = ";
    else
        out << "  ";
    out << opcode( insn.opcode ) << " ";
    int skip = 0;

    if ( insn.opcode == llvm::Instruction::Call )
    {
        skip = 1;
        out << "@" << insn.op->getOperand( insn.op->getNumOperands() - 1 )->getName().str() << " ";
    }

    for ( int i = 1; i < int( insn.op->getNumOperands() + 1 ) - skip; ++i )
    {
        auto val = insn.op->getOperand( i - 1 );
        auto oname = val->getName().str();
        if ( auto I = llvm::dyn_cast< llvm::Instruction >( val ) )
            oname = "%" + ( oname.empty() ?
                            brick::string::fmt( eval.program().pcmap[ I ].instruction() ) : oname );
        else if ( auto B = llvm::dyn_cast< llvm::BasicBlock >( val ) )
            oname = "label %" + ( oname.empty() ?
                                  brick::string::fmt( eval.program().blockmap[ B ].instruction() ) : oname );
        else if ( !oname.empty() )
            oname = "%" + oname;

        if ( oname.empty() )
        {
            if ( insn.value( i ).type == Eval::Slot::Aggregate )
                oname = "<aggregate>";
            else eval.template op<>( i, [&]( auto v )
            {
                oname = brick::string::fmt( v.get( i ) );
            } );
        }

        out << ( oname.empty() ? "?" : oname ) << " ";
    }

    if ( insn.result().type != Eval::Slot::Aggregate )
        eval.template op<>( 0, [&]( auto v )
        {
            int col = out.str().size();
            if ( col >= 45 )
                col = -padding, out << std::endl;
            pad( out, col, 45 );
            out << " # " << brick::string::fmt( v.get( 0 ) );
        } );

    return out.str();
}

template< typename Heap >
std::string raw( Heap &heap, HeapPointer hloc, int sz )
{
    std::stringstream out;

    auto bytes = heap.unsafe_bytes( hloc, hloc.offset(), sz );
    auto types = heap.type( hloc, hloc.offset(), sz );
    auto defined = heap.defined( hloc, hloc.offset(), sz );

    for ( int c = 0; c < ( sz / 12 ) + ( sz % 12 ? 1 : 0 ); ++c )
    {
        int col = 0;
        for ( int i = c * 12; i < std::min( (c + 1) * 12, sz ); ++i )
            print::hexbyte( out, col, i, bytes[ i ] );
        print::pad( out, col, 30 ); out << "| ";
        for ( int i = c * 12; i < std::min( (c + 1) * 12, sz ); ++i )
            print::hexbyte( out, col, i, defined[ i ] );
        print::pad( out, col, 60 ); out << "| ";
        for ( int i = c * 12; i < std::min( (c + 1) * 12, sz ); ++i )
            print::ascbyte( out, col, bytes[ i ] );
        print::pad( out, col, 72 ); out << " | ";
        for ( int i = c * 12; i < std::min( (c + 1) * 12, sz ); ++i )
            print::ascbyte( out, col, types[ i ] );
        print::pad( out, col, 84 );
        if ( c + 1 < ( sz / 12 ) + ( sz % 12 ? 1 : 0 ) )
            out << std::endl;
    }

    return out.str();
}

static std::string demangle( std::string mangled )
{
    int stat;
    auto x = abi::__cxa_demangle( mangled.c_str(), nullptr, nullptr, &stat );
    auto ret = stat == 0 && x ? std::string( x ) : mangled;
    std::free( x );
    return ret;
}

template< typename Program >
static std::string source( llvm::DISubprogram *di, Program &program, CodePointer pc )
{
    std::stringstream out;

    brick::string::Splitter split( "\n", REG_EXTENDED );
    auto src = cc::runtime::source( di->getFilename() );
    if ( src.empty() )
        src = brick::fs::readFile( di->getFilename() );

    auto line = split.begin( src );
    unsigned lineno = 1, active = 0;
    while ( lineno < di->getLine() )
        ++ line, ++ lineno;
    unsigned endline = lineno;

    /* figure out the source code span the function covers; painfully */
    for ( auto &i : program.function( di->getFunction()  ).instructions )
    {
        if ( !i.op )
            continue;
        auto op = llvm::cast< llvm::Instruction >( i.op );
        auto dl = op->getDebugLoc().get();
        if ( !dl )
            continue;
        dl = dl->getInlinedAt() ?: dl;
        if ( program.instruction( pc ).op == i.op )
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

    return out.str();
}

}

}
}
