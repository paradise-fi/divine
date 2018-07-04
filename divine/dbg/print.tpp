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

#include <divine/dbg/print.hpp>
#include <divine/vm/eval.tpp>
#include <brick-llvm>

using namespace std::literals;

namespace divine::dbg::print
{

template< typename Ctx >
std::string Print< Ctx >::value( llvm::Value *val, DisplayVal disp )
{
    std::stringstream num2str;
    num2str << std::setw( 2 ) << std::setfill( '0' ) << std::hex;

    std::string name;

    if ( disp != DisplayVal::Value && val )
    {
        if ( auto I = llvm::dyn_cast< llvm::Instruction >( val ) )
        {
            num2str << dbg.find( I, vm::CodePointer() ).second.instruction();
            name = "%" + num2str.str();
        }
        else if ( auto B = llvm::dyn_cast< llvm::BasicBlock >( val ) )
        {
            auto insn = B->begin();
            ASSERT( insn != B->end() );
            num2str << dbg.find( &*insn, vm::CodePointer() ).second.instruction() - 1;
            name = B->getName().str();
            name = "label %" + ( name.empty() || name.size() > 20 ? num2str.str() : name );
        }
    }

    if ( name.empty() && disp != DisplayVal::Name )
    {
        auto slot = eval.program().valuemap[ val ];
        if ( auto F = val ? llvm::dyn_cast< llvm::Function >( val ) : nullptr )
            name = F->getName().str();
        else if ( slot.type == lx::Slot::Agg )
            name = "<aggregate>";
        else
            eval.template type_dispatch<>(
                slot.type,
                [&]( auto v ) { name = brick::string::fmt( v.get( slot ) ); }, slot );
    }

    return name;
}

template< typename Ctx >
std::string Print< Ctx >::dbginst( llvm::Instruction *I )
{
    if ( auto DDI = llvm::dyn_cast< llvm::DbgDeclareInst >( I ) )
        return DDI->getVariable()->getName().str();

    if ( auto DDV = llvm::dyn_cast< llvm::DbgValueInst >( I ) )
        return DDV->getVariable()->getName().str() + " " +
               value( DDV->getValue(), DisplayVal::Value );

    if ( auto BI = llvm::dyn_cast< llvm::BitCastInst >( I ) )
    {
        std::string out;
        llvm::raw_string_ostream ostr( out );
        ostr << "to " << *(BI->getType());
        return ostr.str();
    }

    UNREACHABLE( "dbginst called on a bad instruction type:", I );
}

template< typename Ctx >
std::string Print< Ctx >::instruction( int padding, int colmax )
{
    std::stringstream out;
    auto &insn = eval.instruction();

    auto I = dbg.find( nullptr, eval.pc() ).first;
    if ( !I )
        return opcode( insn );

    bool printres = true;

    if ( insn.result().type != lx::Slot::Void )
        out << value( I, DisplayVal::Name ) << " = ";
    else
        printres = false;

    out << opcode( insn ) << " ";
    uint64_t skipMask = 0;

    int argc = I->getNumOperands();
    if ( insn.opcode == lx::OpDbg && insn.subcode != lx::DbgBitCast )
        argc = 0;

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
            auto oname = value( val, DisplayVal::PreferName );
            argcols = oname.size() + 1;
            out << ( oname.empty() ? "?" : oname ) << " ";
        }
    }

    auto result = [&]( int col )
                  {
                      while ( col < 60 )
                          col ++, out << " ";
                      out << "# " << value( dbg.find( nullptr, eval.pc() ).first, DisplayVal::Value );
                  };

    for ( int i = 0; i < argc; ++i )
    {
        if ( ( (uint64_t( 1 ) << i) & skipMask ) != 0 )
            continue;

        auto val = I->getOperand( i );
        auto oname = value( val, DisplayVal::PreferName );

        int cols = argalign + argcols + oname.size() + 1;
        if ( ( printres && cols >= colmax - 20 ) || cols >= colmax )
        {
            if ( printres )
                printres = false, result( argalign + argcols );
            out << std::endl;
            for ( int i = 0; i < argalign; ++i ) out << " ";
            argcols = 0;
        }
        argcols += oname.size() + 1;

        out << ( oname.empty() ? "?" : oname ) << " ";
    }

    if ( insn.opcode == lx::OpDbg )
    {
        auto str = dbginst( I );
        argcols += str.size() + 1;
        out << str << " ";
    }

    if ( printres )
        result( argalign + argcols );

    return out.str();
}

}

// vim: syntax=cpp tabstop=4 shiftwidth=4 expandtab ft=cpp
