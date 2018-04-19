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

#include <divine/dbg/print.tpp>
#include <divine/dbg/context.hpp>
#include <divine/vm/eval.tpp>

using namespace std::literals;

namespace divine::dbg::print
{

#define HANDLE_INST(num, opc, class) [num] = #opc ## s,
static std::string _opcode[] = { "",
#include <llvm/IR/Instruction.def>
};
#undef HANDLE_INST

void opcode_tolower()
{
    static bool done = false;
    if ( done )
        return;
    for ( int i = 0; i < int( sizeof( _opcode ) / sizeof( _opcode[0] ) ); ++i )
        std::transform( _opcode[i].begin(), _opcode[i].end(),
                        _opcode[i].begin(), ::tolower );
    done = true;
}

std::string opcode( int op )
{
    if ( op == lx::OpDbgCall )
        return "dbg.call";
    if ( op == lx::OpDbg )
        return "dbg";
    if ( op == lx::OpHypercall )
        return "vm";
    opcode_tolower();
    return _opcode[ op ];
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
    if ( insn.opcode == lx::OpDbg )
        switch ( insn.subcode )
        {
            case lx::DbgValue:   op += ".value"; break;
            case lx::DbgDeclare: op += ".declare"; break;
            case lx::DbgBitCast: op += ".bitcast"; break;
            default: UNREACHABLE( "unexpected debug opcode" ); break;
        }
    if ( insn.opcode == lx::OpHypercall )
        switch ( insn.subcode )
        {
            case lx::HypercallChoose: op += ".choose"; break;
            case lx::HypercallFault: op += ".fault"; break;

            case lx::HypercallCtlSet: op += ".ctl.set"; break;
            case lx::HypercallCtlGet: op += ".ctl.get"; break;
            case lx::HypercallCtlFlag: op += ".ctl.flag"; break;

            case lx::HypercallTestCrit: op += ".test.crit"; break;
            case lx::HypercallTestLoop: op += ".test.loop"; break;
            case lx::HypercallTestTaint: op += ".test.taint"; break;

            case lx::HypercallPeek: op += ".peek"; break;
            case lx::HypercallPoke: op += ".poke"; break;

            case lx::HypercallTrace : op += ".trace"; break;
            case lx::HypercallSyscall: op += ".syscall"; break;

            case lx::HypercallObjMake: op += ".obj.make"; break;
            case lx::HypercallObjFree: op += ".obj.free"; break;
            case lx::HypercallObjShared: op += ".obj.shared"; break;
            case lx::HypercallObjResize: op += ".obj.resize"; break;
            case lx::HypercallObjSize: op += ".obj.size"; break;
            case lx::HypercallObjClone: op += ".obj.clone"; break;

            default: UNREACHABLE( "unexpected hypercall opcode" ); break;
        }
    return op;
}

template< typename Heap >
std::string raw( Heap &heap, vm::HeapPointer hloc, int sz )
{
    std::stringstream out;

    auto bytes = heap.unsafe_bytes( hloc, sz );
    /*
    auto types = heap.type( hloc, hloc.offset(), sz );
    auto defined = heap.defined( hloc, hloc.offset(), sz );
    */

    for ( int c = 0; c < ( sz / 12 ) + ( sz % 12 ? 1 : 0 ); ++c )
    {
        int col = 0;
        for ( int i = c * 12; i < std::min( (c + 1) * 12, sz ); ++i )
            print::hexbyte( out, col, i, bytes[ i ] );
        /*
        print::pad( out, col, 30 ); out << "| ";
        for ( int i = c * 12; i < std::min( (c + 1) * 12, sz ); ++i )
            print::hexbyte( out, col, i, defined[ i ] );
        print::pad( out, col, 60 ); out << "| ";
        for ( int i = c * 12; i < std::min( (c + 1) * 12, sz ); ++i )
            print::ascbyte( out, col, bytes[ i ] );
        print::pad( out, col, 72 ); out << " | ";
        for ( int i = c * 12; i < std::min( (c + 1) * 12, sz ); ++i )
            print::ascbyte( out, col, types[ i ] );
        */
        print::pad( out, col, 84 );
        if ( c + 1 < ( sz / 12 ) + ( sz % 12 ? 1 : 0 ) )
            out << std::endl;
    }

    return out.str();
}

// template struct Print< vm::Context< vm::CowHeap > >;
template struct Print< dbg::Context< vm::CowHeap > >;
template struct Print< dbg::DNContext< vm::CowHeap > >;
template std::string raw< vm::CowHeap >( vm::CowHeap &, vm::HeapPointer, int );
template std::string opcode< vm::Program::Instruction >( vm::Program::Instruction & );

}
