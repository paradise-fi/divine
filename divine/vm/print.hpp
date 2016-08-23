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

#include <divine/vm/eval.hpp>

namespace divine {
namespace vm {

namespace print {

std::string opcode( int );

static void pad( std::ostream &o, int &col, int target )
{
    while ( col < target )
        ++col, o << " ";
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
            else eval.template op< Any >( i, [&]( auto v )
            {
                oname = brick::string::fmt( v.get( i ) );
            } );
        }

        out << ( oname.empty() ? "?" : oname ) << " ";
    }

    if ( insn.result().type != Eval::Slot::Aggregate )
        eval.template op< Any >( 0, [&]( auto v )
        {
            int col = out.str().size();
            if ( col >= 45 )
                col = -padding, out << std::endl;
            pad( out, col, 45 );
            out << " # " << brick::string::fmt( v.get( 0 ) );
        } );

    return out.str();
}

}

}
}
