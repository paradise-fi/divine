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
#include <divine/vm/heap.hpp>
#include <divine/vm/lx-slot.hpp>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Instructions.h>
DIVINE_UNRELAX_WARNINGS

namespace divine::vm::lx
{

enum OpCodes { OpHypercall = llvm::Instruction::OtherOpsEnd + 1, OpBB, OpArg, OpDbg, OpDbgCall };

enum Hypercall /* see divm.h for prototypes & documentation */
{
    NotHypercall = 0,
    NotHypercallButIntrinsic = 1,

    HypercallControl,
    HypercallChoose,
    HypercallFault,

    HypercallTestCrit,
    HypercallTestLoop,
    HypercallTestTaint,

    HypercallCtlSet,
    HypercallCtlGet,
    HypercallCtlFlag,

    /* feedback */
    HypercallTrace,
    HypercallSyscall,

    /* memory management */
    HypercallObjMake,
    HypercallObjClone,
    HypercallObjFree,
    HypercallObjShared,
    HypercallObjResize,
    HypercallObjSize
};

enum DbgSubcode
{
    DbgValue, DbgDeclare, DbgBitCast
};
}
