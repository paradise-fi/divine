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

namespace {

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
