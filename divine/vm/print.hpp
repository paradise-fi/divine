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

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Instructions.h>
#include <llvm/IR/CallSite.h>
DIVINE_UNRELAX_WARNINGS

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
            num2str << eval.program().find( I, CodePointer() ).second.instruction();
            name = "%" + num2str.str();
        }
        else if ( auto B = llvm::dyn_cast< llvm::BasicBlock >( val ) )
        {
            auto insn = eval.program().first_indexed( B->begin(), B->end() );
            ASSERT( insn != B->end() );
            num2str << eval.program().find( &*insn, CodePointer() ).second.instruction() - 1;
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
    out << "# " << value( eval, eval.program().find( nullptr, eval.pc() ).first, DisplayVal::Value );
}

template< typename I >
decltype( I::opcode, std::string() ) opcode( I &insn )
{
    std::string op = opcode( insn.opcode );
    if ( insn.opcode == llvm::Instruction::ICmp )
        switch ( insn.subcode )
        {
            case llvm::ICmpInst::ICMP_EQ: op += ".eq"; break;
            case llvm::ICmpInst::ICMP_NE: op += ".ne"; break;
            case llvm::ICmpInst::ICMP_ULT: op += ".ult"; break;
            case llvm::ICmpInst::ICMP_UGE: op += ".uge"; break;
            case llvm::ICmpInst::ICMP_UGT: op += ".ugt"; break;
            case llvm::ICmpInst::ICMP_ULE: op += ".ule"; break;
            case llvm::ICmpInst::ICMP_SLT: op += ".slt"; break;
            case llvm::ICmpInst::ICMP_SGT: op += ".sgt"; break;
            case llvm::ICmpInst::ICMP_SLE: op += ".sle"; break;
            case llvm::ICmpInst::ICMP_SGE: op += ".sge"; break;
            default: UNREACHABLE( "unexpected icmp predicate" ); break;
        }
    if ( insn.opcode == llvm::Instruction::FCmp )
        switch ( insn.subcode )
        {
            case llvm::FCmpInst::FCMP_OEQ: op += ".oeq"; break;
            case llvm::FCmpInst::FCMP_ONE: op += ".one"; break;
            case llvm::FCmpInst::FCMP_OLE: op += ".ole"; break;
            case llvm::FCmpInst::FCMP_OLT: op += ".olt"; break;
            case llvm::FCmpInst::FCMP_OGE: op += ".oge"; break;
            case llvm::FCmpInst::FCMP_OGT: op += ".ogt"; break;

            case llvm::FCmpInst::FCMP_UEQ: op += ".ueq"; break;
            case llvm::FCmpInst::FCMP_UNE: op += ".une"; break;
            case llvm::FCmpInst::FCMP_ULE: op += ".ule"; break;
            case llvm::FCmpInst::FCMP_ULT: op += ".ult"; break;
            case llvm::FCmpInst::FCMP_UGE: op += ".uge"; break;
            case llvm::FCmpInst::FCMP_UGT: op += ".ugt"; break;

            case llvm::FCmpInst::FCMP_FALSE: op += ".false"; break;
            case llvm::FCmpInst::FCMP_TRUE: op += ".true"; break;
            case llvm::FCmpInst::FCMP_ORD: op += ".ord"; break;
            case llvm::FCmpInst::FCMP_UNO: op += ".uno"; break;
            default: UNREACHABLE( "unexpected fcmp predicate" ); break;
        }
    if ( insn.opcode == OpDbg )
        switch ( insn.subcode )
        {
            case llvm::Intrinsic::dbg_value: op += ".value"; break;
            case llvm::Intrinsic::dbg_declare: op += ".declare"; break;
            default: UNREACHABLE( "unexpected debug opcode" ); break;
        }
    if ( insn.opcode == OpHypercall )
        switch ( insn.subcode )
        {
            case HypercallControl: op += ".control"; break;
            case HypercallChoose: op += ".choose"; break;
            case HypercallFault: op += ".fault"; break;

            case HypercallInterruptCfl: op += ".interrupt.cfl"; break;
            case HypercallInterruptMem: op += ".interrupt.mem"; break;

            case HypercallTrace : op += ".trace"; break;
            case HypercallSyscall: op += ".syscall"; break;

            case HypercallObjMake: op += ".obj.make"; break;
            case HypercallObjFree: op += ".obj.free"; break;
            case HypercallObjShared: op += ".obj.shared"; break;
            case HypercallObjResize: op += ".obj.resize"; break;
            case HypercallObjSize: op += ".obj.size"; break;
            case HypercallObjClone: op += ".obj.clone"; break;

            default: UNREACHABLE( "unexpected debug opcode" ); break;
        }
    return op;
}

template< typename Eval >
static std::string instruction( Eval &eval, int padding = 0, int colmax = 80 )
{
    std::stringstream out;
    auto &insn = eval.instruction();
    auto I = eval.program().find( nullptr, eval.pc() ).first;
    ASSERT( I );

    bool printres = true;

    if ( insn.result().type != Program::Slot::Void )
        out << value( eval, I, DisplayVal::Name ) << " = ";
    else
        printres = false;

    out << opcode( insn ) << " ";
    uint64_t skipMask = 0;
    const int argc = insn.opcode == OpDbg ? 0 : I->getNumOperands();
    int argalign = out.str().size() + padding, argcols = 0;

    if ( insn.opcode == llvm::Instruction::Call || insn.opcode == llvm::Instruction::Invoke )
    {
        llvm::CallSite cs( I );

        skipMask |= uint64_t( 1 ) << (argc - (cs.isCall() ? 1 : 3));
        if ( auto *target = cs.getCalledFunction() ) {
            auto tgt = target->getName().str();
            argcols = tgt.size() + 2;
            out << "@" << tgt << " ";
        } else {
            auto *val = cs.getCalledValue();
            auto oname = value( eval, val, DisplayVal::PreferName );
            argcols = oname.size() + 1;
            out << ( oname.empty() ? "?" : oname ) << " ";
        }
    }

    for ( int i = 0; i < argc; ++i )
    {
        if ( ( (uint64_t( 1 ) << i) & skipMask ) != 0 )
            continue;

        auto val = I->getOperand( i );
        auto oname = value( eval, val, DisplayVal::PreferName );

        int cols = argalign + argcols + oname.size() + 1;
        if ( ( printres && cols >= colmax - 20 ) || cols >= colmax )
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
    std::string src( rt::source( di->getFilename() ) );
    if ( src.empty() )
        src = brick::fs::readFile( di->getFilename() );

    auto line = split.begin( src );
    unsigned lineno = 1, active = 0;
    while ( lineno < di->getLine() )
        ++ line, ++ lineno;
    unsigned endline = lineno;

    CodePointer iter( pc.function(), 0 );

    /* figure out the source code span the function covers; painfully */
    for ( iter = program.nextpc( iter ); program.valid( iter ); iter = program.advance( iter ) )
    {
        auto dl = program.find( nullptr, iter ).first->getDebugLoc().get();
        if ( !dl )
            continue;
        while ( dl->getInlinedAt() )
            dl = dl->getInlinedAt();
        if ( iter >= pc && dl && !active )
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
