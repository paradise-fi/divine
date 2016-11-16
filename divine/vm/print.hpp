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
#include <divine/rt/runtime.hpp>

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

enum class DisplayVal { Name, Value, PreferName };

template< typename Eval >
static std::string value( Eval &eval, llvm::Value *val, DisplayVal disp = DisplayVal::PreferName )
{
    std::stringstream num2str;
    num2str << std::setw( 2 ) << std::setfill( '0' ) << std::hex;

    std::string name;

    if ( disp != DisplayVal::Value )
    {
        if ( auto I = llvm::dyn_cast< llvm::Instruction >( val ) )
        {
            num2str << eval.program().pcmap[ I ].instruction();
            name = "%" + num2str.str();
        }
        else if ( auto B = llvm::dyn_cast< llvm::BasicBlock >( val ) )
        {
            num2str << eval.program().blockmap[ B ].instruction();
            name = B->getName().str();
            name = "label %" + ( name.empty() || name.size() > 20 ? num2str.str() : name );
        }
    }

    if ( name.empty() && disp != DisplayVal::Name )
    {
        auto slot = eval.program().valuemap[ val ].slot;
        if ( slot.type == Eval::Slot::Aggregate )
            name = "<aggregate>";
        else
            eval.template type_dispatch<>(
                slot.width(), slot.type,
                [&]( auto v ) { name = brick::string::fmt( v.get( slot ) ); } );
    }

    return name;
}

template< typename Eval >
static void result( std::ostream &out, int col, Eval &eval )
{
    while ( col < 60 )
        col ++, out << " ";
    out << "# " << value( eval, eval.instruction().op, DisplayVal::Value );
}

template< typename Eval >
static std::string instruction( Eval &eval, int padding = 0 )
{
    std::stringstream out;
    auto &insn = eval.instruction();
    ASSERT( insn.op );

    bool printres = true;

    if ( insn.result().type != Program::Slot::Void )
        out << value( eval, insn.op, DisplayVal::Name ) << " = ";
    else
        printres = false;

    out << opcode( insn.opcode ) << " ";
    int skip = 0;
    int argalign = out.str().size() + padding, argcols = 0;

    if ( insn.opcode == llvm::Instruction::Call )
    {
        skip = 1;
        auto tgt = insn.op->getOperand( insn.op->getNumOperands() - 1 )->getName().str();
        argcols = tgt.size() + 2;
        out << "@" << tgt << " ";
    }

    for ( int i = 1; i < int( insn.op->getNumOperands() + 1 ) - skip; ++i )
    {
        auto val = insn.op->getOperand( i - 1 );
        auto oname = value( eval, val, DisplayVal::PreferName );

        int cols = argalign + argcols + oname.size() + 1;
        if ( ( printres && cols >= 60 ) || cols >= 80 )
        {
            if ( printres )
                printres = false, result( out, argalign + argcols, eval );
            out << std::endl;
            for ( int i = 0; i < argalign; ++i ) out << " ";
            argcols = 0;
        }
        argcols += oname.size() + 1;

        out << ( oname.empty() ? "?" : oname ) << " ";
    }

    if ( printres )
        result( out, argalign + argcols, eval );

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

    brick::string::Splitter split( "\n", std::regex::extended );
    auto src = rt::source( di->getFilename() );
    if ( src.empty() )
        src = brick::fs::readFile( di->getFilename() );

    auto line = split.begin( src );
    unsigned lineno = 1, active = 0;
    while ( lineno < di->getLine() )
        ++ line, ++ lineno;
    unsigned endline = lineno;

    auto act_op = program.instruction( pc ).op;
    if ( !act_op )
        act_op = program.instruction( pc + 1 ).op;

    /* figure out the source code span the function covers; painfully */
    for ( auto &i : program.function( pc ).instructions )
    {
        if ( !i.op )
            continue;
        auto op = llvm::cast< llvm::Instruction >( i.op );
        auto dl = op->getDebugLoc().get();
        if ( !dl )
            continue;
        while ( dl->getInlinedAt() )
            dl = dl->getInlinedAt();
        if ( i.op == act_op )
            active = dl->getLine();
        endline = std::max( endline, dl->getLine() );
    }

    /* print it */
    while ( line != split.end() && lineno <= endline )
    {
        out << (lineno == active ? ">>" : "  ");
            out << std::setw( 5 ) << lineno++ << " " << *line++ << std::endl;
    }
    if ( line != split.end() ) {
        std::regex endbrace( "^[ \t]*}", std::regex::extended );
        if ( std::regex_search( line->str(), endbrace ) )
            out << "  " << std::setw( 5 ) << lineno++ << " " << *line++ << std::endl;
    }

    return out.str();
}

}

}
}
