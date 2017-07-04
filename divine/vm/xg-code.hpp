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

#include <brick-bitlevel>
#include <divine/vm/divm.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/GlobalAlias.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/CallSite.h>
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

#include <divine/vm/lx-code.hpp>

/*
 * RR generation goes like this:
 * - assign addresses to basic blocks
 * - assign addresses to slots
 * - compute constants (consulting the above assignments)
 * - generate the image for initial mutable globals
 * - generate the instruction stream
 */

namespace divine::vm::xg
{


/*
 * The address map keeps track of things that can have their address taken.
 */
struct AddressMap
{
    std::map< llvm::BasicBlock *, CodePointer > _code, _terminator;
    std::map< llvm::Constant *, GlobalPointer > _const;
    std::map< llvm::GlobalVariable *, GlobalPointer > _mutable;

    CodePointer code( llvm::BasicBlock *bb )
    {
        ASSERT( _code.count( bb ) );
        return _code[ bb ];
    }

    CodePointer code( llvm::Function *f )
    {
        return addr( &*f->begin() );
    }

    CodePointer terminator( llvm::BasicBlock *bb )
    {
        ASSERT( _terminator.count( bb ) );
        return _terminator[ bb ];
    }

    GlobalPointer constant( llvm::Constant *v )
    {
        if ( auto GA = llvm::dyn_cast< llvm::GlobalAlias >( v ) )
            return constant( GA->getBaseObject() );
        ASSERT( _const.count( v ) );
        return _const[ v ];
    }

    GlobalPointer mut( llvm::GlobalVariable *v )
    {
        ASSERT( _mutable.count( v ) );
        return _mutable[ v ];
    }

    template< typename F >
    void each_const( F )
    {
        NOT_IMPLEMENTED();
    }

    template< typename F >
    void each_mutable( F )
    {
        NOT_IMPLEMENTED();
    }

    GenericPointer addr( llvm::Value *v )
    {
        if ( auto GA = llvm::dyn_cast< llvm::GlobalAlias >( v ) )
            return addr( GA->getBaseObject() );
        if ( auto F = llvm::dyn_cast< llvm::Function >( v ) )
            return code( F );
        if ( auto B = llvm::dyn_cast< llvm::BasicBlock >( v ) )
            return code( B );
        if ( auto B = llvm::dyn_cast< llvm::BlockAddress >( v ) )
            return code( B->getBasicBlock() );

        if ( auto C = llvm::dyn_cast< llvm::Constant >( v ) )
            return constant( C );

        UNREACHABLE( "impossible value type" );
    }

    void add( llvm::BasicBlock *b, CodePointer p )
    {
        _code[ b ] = p;
    }

    void build( llvm::Module *m )
    {
        CodePointer pc( 0, 0 );
        for ( auto &f : *m )
        {
            if ( f.isDeclaration() )
                continue;
            pc.function( pc.function() + 1 );
            pc.instruction( 0 );

            for ( auto &b : f )
            {
                _code[ &b ] = pc;
                for ( auto &i : b )
                    pc.instruction( pc.instruction() + 1 ), void(i);
                _terminator[ &b ] = pc;
                pc.instruction( pc.instruction() + 1 );
            }
        }
    }
};

namespace
{

static int intrinsic_id( llvm::Value *v )
{
    auto insn = llvm::dyn_cast< llvm::Instruction >( v );
    if ( !insn || insn->getOpcode() != llvm::Instruction::Call )
        return llvm::Intrinsic::not_intrinsic;
    llvm::CallSite CS( insn );
    auto f = CS.getCalledFunction();
    if ( !f )
        return llvm::Intrinsic::not_intrinsic;
    return f->getIntrinsicID();
}

lx::Hypercall hypercall( llvm::Function *f )
{
    std::string name = f->getName().str();

    if ( name == "__vm_control" )
        return lx::HypercallControl;
    if ( name == "__vm_choose" )
        return lx::HypercallChoose;
    if ( name == "__vm_fault" )
        return lx::HypercallFault;

    if ( name == "__vm_interrupt_cfl" )
        return lx::HypercallInterruptCfl;
    if ( name == "__vm_interrupt_mem" )
        return lx::HypercallInterruptMem;

    if ( name == "__vm_trace" )
        return lx::HypercallTrace;
    if ( name == "__vm_syscall" )
        return lx::HypercallSyscall;

    if ( name == "__vm_obj_make" )
        return lx::HypercallObjMake;
    if ( name == "__vm_obj_clone" )
        return lx::HypercallObjClone;
    if ( name == "__vm_obj_free" )
        return lx::HypercallObjFree;
    if ( name == "__vm_obj_shared" )
        return lx::HypercallObjShared;
    if ( name == "__vm_obj_resize" )
        return lx::HypercallObjResize;
    if ( name == "__vm_obj_size" )
        return lx::HypercallObjSize;

    if ( f->getIntrinsicID() != llvm::Intrinsic::not_intrinsic )
        return lx::NotHypercallButIntrinsic;

    return lx::NotHypercall;
}

_VM_Operand::Type type( llvm::Type *t )
{
    if ( t->isVoidTy() )
        return _VM_Operand::Void;

    if ( t->isPointerTy() )
        return _VM_Operand::Ptr;

    if ( t->isIntegerTy() )
        switch ( t->getPrimitiveSizeInBits() )
        {
            case  1: return _VM_Operand::I1;
            case  8: return _VM_Operand::I8;
            case 16: return _VM_Operand::I16;
            case 32: return _VM_Operand::I32;
            case 64: return _VM_Operand::I64;
            default: return _VM_Operand::Other;
        }

    if ( t->isFloatingPointTy() )
        switch ( t->getPrimitiveSizeInBits() )
        {
            case 32: return _VM_Operand::F32;
            case 64: return _VM_Operand::F64;
            case 80: return _VM_Operand::F80;
            default: return _VM_Operand::Other;
        }

    return _VM_Operand::Agg;
}

}

}
