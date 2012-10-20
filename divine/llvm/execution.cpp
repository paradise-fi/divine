//===-- Execution.cpp - Implement code to simulate the program ------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file contains the actual instruction interpreter.
//
//===----------------------------------------------------------------------===//

#include <divine/llvm/interpreter.h>

#include "llvm/Constants.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Instructions.h"
#include "llvm/CodeGen/IntrinsicLowering.h"
#include "llvm/Support/GetElementPtrTypeIterator.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Analysis/DebugInfo.h"
#include <algorithm>
#include <cmath>
#include <wibble/test.h>
#include <wibble/sys/mutex.h>

using namespace llvm;
using namespace divine::llvm;

static cl::opt<bool> PrintVolatile("interpreter-print-volatile", cl::Hidden,
          cl::desc("make the interpreter print every volatile load and store"));

//===----------------------------------------------------------------------===//
//                     Various Helper Functions
//===----------------------------------------------------------------------===//

void Interpreter::SetValue(Value *V, GenericValue Val, ExecutionContext &SF) {
    SF.values[valueIndex.left(V)] = Val;
}

//===----------------------------------------------------------------------===//
//                    Binary Instruction Implementations
//===----------------------------------------------------------------------===//

#define IMPLEMENT_BINARY_OPERATOR(OP, TY) \
   case Type::TY##TyID: \
     Dest.TY##Val = Src1.TY##Val OP Src2.TY##Val; \
     break

static void executeFAddInst(GenericValue &Dest, GenericValue Src1,
                            GenericValue Src2, Type *Ty) {
  switch (Ty->getTypeID()) {
    IMPLEMENT_BINARY_OPERATOR(+, Float);
    IMPLEMENT_BINARY_OPERATOR(+, Double);
  default:
      assert_die();
  }
}

static void executeFSubInst(GenericValue &Dest, GenericValue Src1,
                            GenericValue Src2, Type *Ty) {
  switch (Ty->getTypeID()) {
    IMPLEMENT_BINARY_OPERATOR(-, Float);
    IMPLEMENT_BINARY_OPERATOR(-, Double);
  default:
      assert_die();
  }
}

static void executeFMulInst(GenericValue &Dest, GenericValue Src1,
                            GenericValue Src2, Type *Ty) {
  switch (Ty->getTypeID()) {
    IMPLEMENT_BINARY_OPERATOR(*, Float);
    IMPLEMENT_BINARY_OPERATOR(*, Double);
  default:
      assert_die();
  }
}

static void executeFDivInst(GenericValue &Dest, GenericValue Src1, 
                            GenericValue Src2, Type *Ty) {
  switch (Ty->getTypeID()) {
    IMPLEMENT_BINARY_OPERATOR(/, Float);
    IMPLEMENT_BINARY_OPERATOR(/, Double);
  default:
      assert_die();
  }
}

static void executeFRemInst(GenericValue &Dest, GenericValue Src1, 
                            GenericValue Src2, Type *Ty) {
  switch (Ty->getTypeID()) {
  case Type::FloatTyID:
    Dest.FloatVal = fmod(Src1.FloatVal, Src2.FloatVal);
    break;
  case Type::DoubleTyID:
    Dest.DoubleVal = fmod(Src1.DoubleVal, Src2.DoubleVal);
    break;
  default:
      assert_die();
  }
}

#define IMPLEMENT_INTEGER_ICMP(OP, TY) \
   case Type::IntegerTyID:  \
      Dest.IntVal = APInt(1,Src1.IntVal.OP(Src2.IntVal)); \
      break;

// Handle pointers specially because they must be compared with only as much
// width as the host has.  We _do not_ want to be comparing 64 bit values when
// running on a 32-bit target, otherwise the upper 32 bits might mess up
// comparisons if they contain garbage.
#define IMPLEMENT_POINTER_ICMP(OP) \
   case Type::PointerTyID: \
      Dest.IntVal = APInt(1,(void*)(intptr_t)Src1.PointerVal OP \
                            (void*)(intptr_t)Src2.PointerVal); \
      break;

static GenericValue executeICMP_EQ(GenericValue Src1, GenericValue Src2,
                                   Type *Ty) {
  GenericValue Dest;
  switch (Ty->getTypeID()) {
    IMPLEMENT_INTEGER_ICMP(eq,Ty);
    IMPLEMENT_POINTER_ICMP(==);
  default:
      assert_die();
  }
  return Dest;
}

static GenericValue executeICMP_NE(GenericValue Src1, GenericValue Src2,
                                   Type *Ty) {
  GenericValue Dest;
  switch (Ty->getTypeID()) {
    IMPLEMENT_INTEGER_ICMP(ne,Ty);
    IMPLEMENT_POINTER_ICMP(!=);
  default:
      assert_die();
  }
  return Dest;
}

static GenericValue executeICMP_ULT(GenericValue Src1, GenericValue Src2,
                                    Type *Ty) {
  GenericValue Dest;
  switch (Ty->getTypeID()) {
    IMPLEMENT_INTEGER_ICMP(ult,Ty);
    IMPLEMENT_POINTER_ICMP(<);
  default:
      assert_die();
  }
  return Dest;
}

static GenericValue executeICMP_SLT(GenericValue Src1, GenericValue Src2,
                                    Type *Ty) {
  GenericValue Dest;
  switch (Ty->getTypeID()) {
    IMPLEMENT_INTEGER_ICMP(slt,Ty);
    IMPLEMENT_POINTER_ICMP(<);
  default:
      assert_die();
  }
  return Dest;
}

static GenericValue executeICMP_UGT(GenericValue Src1, GenericValue Src2,
                                    Type *Ty) {
  GenericValue Dest;
  switch (Ty->getTypeID()) {
    IMPLEMENT_INTEGER_ICMP(ugt,Ty);
    IMPLEMENT_POINTER_ICMP(>);
  default:
      assert_die();
  }
  return Dest;
}

static GenericValue executeICMP_SGT(GenericValue Src1, GenericValue Src2,
                                    Type *Ty) {
  GenericValue Dest;
  switch (Ty->getTypeID()) {
    IMPLEMENT_INTEGER_ICMP(sgt,Ty);
    IMPLEMENT_POINTER_ICMP(>);
  default:
      assert_die();
  }
  return Dest;
}

static GenericValue executeICMP_ULE(GenericValue Src1, GenericValue Src2,
                                    Type *Ty) {
  GenericValue Dest;
  switch (Ty->getTypeID()) {
    IMPLEMENT_INTEGER_ICMP(ule,Ty);
    IMPLEMENT_POINTER_ICMP(<=);
  default:
      assert_die();
  }
  return Dest;
}

static GenericValue executeICMP_SLE(GenericValue Src1, GenericValue Src2,
                                    Type *Ty) {
  GenericValue Dest;
  switch (Ty->getTypeID()) {
    IMPLEMENT_INTEGER_ICMP(sle,Ty);
    IMPLEMENT_POINTER_ICMP(<=);
  default:
      assert_die();
  }
  return Dest;
}

static GenericValue executeICMP_UGE(GenericValue Src1, GenericValue Src2,
                                    Type *Ty) {
  GenericValue Dest;
  switch (Ty->getTypeID()) {
    IMPLEMENT_INTEGER_ICMP(uge,Ty);
    IMPLEMENT_POINTER_ICMP(>=);
  default:
      assert_die();
  }
  return Dest;
}

static GenericValue executeICMP_SGE(GenericValue Src1, GenericValue Src2,
                                    Type *Ty) {
  GenericValue Dest;
  switch (Ty->getTypeID()) {
    IMPLEMENT_INTEGER_ICMP(sge,Ty);
    IMPLEMENT_POINTER_ICMP(>=);
  default:
      assert_die();
  }
  return Dest;
}

void Interpreter::visitICmpInst(ICmpInst &I) {
  Type *Ty    = I.getOperand(0)->getType();
  GenericValue Src1 = getOperandValue(I.getOperand(0), SF());
  GenericValue Src2 = getOperandValue(I.getOperand(1), SF());
  GenericValue R;   // Result
  
  switch (I.getPredicate()) {
  case ICmpInst::ICMP_EQ:  R = executeICMP_EQ(Src1,  Src2, Ty); break;
  case ICmpInst::ICMP_NE:  R = executeICMP_NE(Src1,  Src2, Ty); break;
  case ICmpInst::ICMP_ULT: R = executeICMP_ULT(Src1, Src2, Ty); break;
  case ICmpInst::ICMP_SLT: R = executeICMP_SLT(Src1, Src2, Ty); break;
  case ICmpInst::ICMP_UGT: R = executeICMP_UGT(Src1, Src2, Ty); break;
  case ICmpInst::ICMP_SGT: R = executeICMP_SGT(Src1, Src2, Ty); break;
  case ICmpInst::ICMP_ULE: R = executeICMP_ULE(Src1, Src2, Ty); break;
  case ICmpInst::ICMP_SLE: R = executeICMP_SLE(Src1, Src2, Ty); break;
  case ICmpInst::ICMP_UGE: R = executeICMP_UGE(Src1, Src2, Ty); break;
  case ICmpInst::ICMP_SGE: R = executeICMP_SGE(Src1, Src2, Ty); break;
  default:
      assert_die();
  }
 
  SetValue(&I, R, SF());
}

#define IMPLEMENT_FCMP(OP, TY) \
   case Type::TY##TyID: \
     Dest.IntVal = APInt(1,Src1.TY##Val OP Src2.TY##Val); \
     break

static GenericValue executeFCMP_OEQ(GenericValue Src1, GenericValue Src2,
                                   Type *Ty) {
  GenericValue Dest;
  switch (Ty->getTypeID()) {
    IMPLEMENT_FCMP(==, Float);
    IMPLEMENT_FCMP(==, Double);
  default:
      assert_die();
  }
  return Dest;
}

static GenericValue executeFCMP_ONE(GenericValue Src1, GenericValue Src2,
                                   Type *Ty) {
  GenericValue Dest;
  switch (Ty->getTypeID()) {
    IMPLEMENT_FCMP(!=, Float);
    IMPLEMENT_FCMP(!=, Double);

  default:
      assert_die();
  }
  return Dest;
}

static GenericValue executeFCMP_OLE(GenericValue Src1, GenericValue Src2,
                                   Type *Ty) {
  GenericValue Dest;
  switch (Ty->getTypeID()) {
    IMPLEMENT_FCMP(<=, Float);
    IMPLEMENT_FCMP(<=, Double);
  default:
      assert_die();
  }
  return Dest;
}

static GenericValue executeFCMP_OGE(GenericValue Src1, GenericValue Src2,
                                   Type *Ty) {
  GenericValue Dest;
  switch (Ty->getTypeID()) {
    IMPLEMENT_FCMP(>=, Float);
    IMPLEMENT_FCMP(>=, Double);
  default:
      assert_die();
  }
  return Dest;
}

static GenericValue executeFCMP_OLT(GenericValue Src1, GenericValue Src2,
                                   Type *Ty) {
  GenericValue Dest;
  switch (Ty->getTypeID()) {
    IMPLEMENT_FCMP(<, Float);
    IMPLEMENT_FCMP(<, Double);
  default:
      assert_die();
  }
  return Dest;
}

static GenericValue executeFCMP_OGT(GenericValue Src1, GenericValue Src2,
                                     Type *Ty) {
  GenericValue Dest;
  switch (Ty->getTypeID()) {
    IMPLEMENT_FCMP(>, Float);
    IMPLEMENT_FCMP(>, Double);
  default:
      assert_die();
  }
  return Dest;
}

#define IMPLEMENT_UNORDERED(TY, X,Y)                                     \
  if (TY->isFloatTy()) {                                                 \
    if (X.FloatVal != X.FloatVal || Y.FloatVal != Y.FloatVal) {          \
      Dest.IntVal = APInt(1,true);                                       \
      return Dest;                                                       \
    }                                                                    \
  } else if (X.DoubleVal != X.DoubleVal || Y.DoubleVal != Y.DoubleVal) { \
    Dest.IntVal = APInt(1,true);                                         \
    return Dest;                                                         \
  }


static GenericValue executeFCMP_UEQ(GenericValue Src1, GenericValue Src2,
                                   Type *Ty) {
  GenericValue Dest;
  IMPLEMENT_UNORDERED(Ty, Src1, Src2)
  return executeFCMP_OEQ(Src1, Src2, Ty);
}

static GenericValue executeFCMP_UNE(GenericValue Src1, GenericValue Src2,
                                   Type *Ty) {
  GenericValue Dest;
  IMPLEMENT_UNORDERED(Ty, Src1, Src2)
  return executeFCMP_ONE(Src1, Src2, Ty);
}

static GenericValue executeFCMP_ULE(GenericValue Src1, GenericValue Src2,
                                   Type *Ty) {
  GenericValue Dest;
  IMPLEMENT_UNORDERED(Ty, Src1, Src2)
  return executeFCMP_OLE(Src1, Src2, Ty);
}

static GenericValue executeFCMP_UGE(GenericValue Src1, GenericValue Src2,
                                   Type *Ty) {
  GenericValue Dest;
  IMPLEMENT_UNORDERED(Ty, Src1, Src2)
  return executeFCMP_OGE(Src1, Src2, Ty);
}

static GenericValue executeFCMP_ULT(GenericValue Src1, GenericValue Src2,
                                   Type *Ty) {
  GenericValue Dest;
  IMPLEMENT_UNORDERED(Ty, Src1, Src2)
  return executeFCMP_OLT(Src1, Src2, Ty);
}

static GenericValue executeFCMP_UGT(GenericValue Src1, GenericValue Src2,
                                     Type *Ty) {
  GenericValue Dest;
  IMPLEMENT_UNORDERED(Ty, Src1, Src2)
  return executeFCMP_OGT(Src1, Src2, Ty);
}

static GenericValue executeFCMP_ORD(GenericValue Src1, GenericValue Src2,
                                     Type *Ty) {
  GenericValue Dest;
  if (Ty->isFloatTy())
    Dest.IntVal = APInt(1,(Src1.FloatVal == Src1.FloatVal && 
                           Src2.FloatVal == Src2.FloatVal));
  else
    Dest.IntVal = APInt(1,(Src1.DoubleVal == Src1.DoubleVal && 
                           Src2.DoubleVal == Src2.DoubleVal));
  return Dest;
}

static GenericValue executeFCMP_UNO(GenericValue Src1, GenericValue Src2,
                                     Type *Ty) {
  GenericValue Dest;
  if (Ty->isFloatTy())
    Dest.IntVal = APInt(1,(Src1.FloatVal != Src1.FloatVal || 
                           Src2.FloatVal != Src2.FloatVal));
  else
    Dest.IntVal = APInt(1,(Src1.DoubleVal != Src1.DoubleVal || 
                           Src2.DoubleVal != Src2.DoubleVal));
  return Dest;
}

void Interpreter::visitFCmpInst(FCmpInst &I) {
  Type *Ty    = I.getOperand(0)->getType();
  GenericValue Src1 = getOperandValue(I.getOperand(0), SF());
  GenericValue Src2 = getOperandValue(I.getOperand(1), SF());
  GenericValue R;   // Result
  
  switch (I.getPredicate()) {
  case FCmpInst::FCMP_FALSE: R.IntVal = APInt(1,false); break;
  case FCmpInst::FCMP_TRUE:  R.IntVal = APInt(1,true); break;
  case FCmpInst::FCMP_ORD:   R = executeFCMP_ORD(Src1, Src2, Ty); break;
  case FCmpInst::FCMP_UNO:   R = executeFCMP_UNO(Src1, Src2, Ty); break;
  case FCmpInst::FCMP_UEQ:   R = executeFCMP_UEQ(Src1, Src2, Ty); break;
  case FCmpInst::FCMP_OEQ:   R = executeFCMP_OEQ(Src1, Src2, Ty); break;
  case FCmpInst::FCMP_UNE:   R = executeFCMP_UNE(Src1, Src2, Ty); break;
  case FCmpInst::FCMP_ONE:   R = executeFCMP_ONE(Src1, Src2, Ty); break;
  case FCmpInst::FCMP_ULT:   R = executeFCMP_ULT(Src1, Src2, Ty); break;
  case FCmpInst::FCMP_OLT:   R = executeFCMP_OLT(Src1, Src2, Ty); break;
  case FCmpInst::FCMP_UGT:   R = executeFCMP_UGT(Src1, Src2, Ty); break;
  case FCmpInst::FCMP_OGT:   R = executeFCMP_OGT(Src1, Src2, Ty); break;
  case FCmpInst::FCMP_ULE:   R = executeFCMP_ULE(Src1, Src2, Ty); break;
  case FCmpInst::FCMP_OLE:   R = executeFCMP_OLE(Src1, Src2, Ty); break;
  case FCmpInst::FCMP_UGE:   R = executeFCMP_UGE(Src1, Src2, Ty); break;
  case FCmpInst::FCMP_OGE:   R = executeFCMP_OGE(Src1, Src2, Ty); break;
  default:
      assert_die();
  }
 
  SetValue(&I, R, SF());
}

static GenericValue executeCmpInst(unsigned predicate, GenericValue Src1, 
                                   GenericValue Src2, Type *Ty) {
  GenericValue Result;
  switch (predicate) {
  case ICmpInst::ICMP_EQ:    return executeICMP_EQ(Src1, Src2, Ty);
  case ICmpInst::ICMP_NE:    return executeICMP_NE(Src1, Src2, Ty);
  case ICmpInst::ICMP_UGT:   return executeICMP_UGT(Src1, Src2, Ty);
  case ICmpInst::ICMP_SGT:   return executeICMP_SGT(Src1, Src2, Ty);
  case ICmpInst::ICMP_ULT:   return executeICMP_ULT(Src1, Src2, Ty);
  case ICmpInst::ICMP_SLT:   return executeICMP_SLT(Src1, Src2, Ty);
  case ICmpInst::ICMP_UGE:   return executeICMP_UGE(Src1, Src2, Ty);
  case ICmpInst::ICMP_SGE:   return executeICMP_SGE(Src1, Src2, Ty);
  case ICmpInst::ICMP_ULE:   return executeICMP_ULE(Src1, Src2, Ty);
  case ICmpInst::ICMP_SLE:   return executeICMP_SLE(Src1, Src2, Ty);
  case FCmpInst::FCMP_ORD:   return executeFCMP_ORD(Src1, Src2, Ty);
  case FCmpInst::FCMP_UNO:   return executeFCMP_UNO(Src1, Src2, Ty);
  case FCmpInst::FCMP_OEQ:   return executeFCMP_OEQ(Src1, Src2, Ty);
  case FCmpInst::FCMP_UEQ:   return executeFCMP_UEQ(Src1, Src2, Ty);
  case FCmpInst::FCMP_ONE:   return executeFCMP_ONE(Src1, Src2, Ty);
  case FCmpInst::FCMP_UNE:   return executeFCMP_UNE(Src1, Src2, Ty);
  case FCmpInst::FCMP_OLT:   return executeFCMP_OLT(Src1, Src2, Ty);
  case FCmpInst::FCMP_ULT:   return executeFCMP_ULT(Src1, Src2, Ty);
  case FCmpInst::FCMP_OGT:   return executeFCMP_OGT(Src1, Src2, Ty);
  case FCmpInst::FCMP_UGT:   return executeFCMP_UGT(Src1, Src2, Ty);
  case FCmpInst::FCMP_OLE:   return executeFCMP_OLE(Src1, Src2, Ty);
  case FCmpInst::FCMP_ULE:   return executeFCMP_ULE(Src1, Src2, Ty);
  case FCmpInst::FCMP_OGE:   return executeFCMP_OGE(Src1, Src2, Ty);
  case FCmpInst::FCMP_UGE:   return executeFCMP_UGE(Src1, Src2, Ty);
  case FCmpInst::FCMP_FALSE: { 
    GenericValue Result;
    Result.IntVal = APInt(1, false);
    return Result;
  }
  case FCmpInst::FCMP_TRUE: {
    GenericValue Result;
    Result.IntVal = APInt(1, true);
    return Result;
  }
  default:
      assert_die();
  }
}

void Interpreter::visitBinaryOperator(BinaryOperator &I) {
  Type *Ty    = I.getOperand(0)->getType();
  GenericValue Src1 = getOperandValue(I.getOperand(0), SF());
  GenericValue Src2 = getOperandValue(I.getOperand(1), SF());
  GenericValue R;   // Result

  switch (I.getOpcode()) {
  case Instruction::Add:   R.IntVal = Src1.IntVal + Src2.IntVal; break;
  case Instruction::Sub:   R.IntVal = Src1.IntVal - Src2.IntVal; break;
  case Instruction::Mul:   R.IntVal = Src1.IntVal * Src2.IntVal; break;
  case Instruction::FAdd:  executeFAddInst(R, Src1, Src2, Ty); break;
  case Instruction::FSub:  executeFSubInst(R, Src1, Src2, Ty); break;
  case Instruction::FMul:  executeFMulInst(R, Src1, Src2, Ty); break;
  case Instruction::FDiv:  executeFDivInst(R, Src1, Src2, Ty); break;
  case Instruction::FRem:  executeFRemInst(R, Src1, Src2, Ty); break;
  case Instruction::UDiv:  R.IntVal = Src1.IntVal.udiv(Src2.IntVal); break;
  case Instruction::SDiv:  R.IntVal = Src1.IntVal.sdiv(Src2.IntVal); break;
  case Instruction::URem:  R.IntVal = Src1.IntVal.urem(Src2.IntVal); break;
  case Instruction::SRem:  R.IntVal = Src1.IntVal.srem(Src2.IntVal); break;
  case Instruction::And:   R.IntVal = Src1.IntVal & Src2.IntVal; break;
  case Instruction::Or:    R.IntVal = Src1.IntVal | Src2.IntVal; break;
  case Instruction::Xor:   R.IntVal = Src1.IntVal ^ Src2.IntVal; break;
  default:
      assert_die();
  }

  SetValue(&I, R, SF());
}

static GenericValue executeSelectInst(GenericValue Src1, GenericValue Src2,
                                      GenericValue Src3) {
  return Src1.IntVal == 0 ? Src3 : Src2;
}

void Interpreter::visitSelectInst(SelectInst &I) {
    GenericValue Src1 = getOperandValue(I.getOperand(0), SF());
    GenericValue Src2 = getOperandValue(I.getOperand(1), SF());
    GenericValue Src3 = getOperandValue(I.getOperand(2), SF());
    GenericValue R = executeSelectInst(Src1, Src2, Src3);
    SetValue(&I, R, SF());
}


//===----------------------------------------------------------------------===//
//                     Terminator Instruction Implementations
//===----------------------------------------------------------------------===//

/// Pop the last stack frame off of ECStack and then copy the result
/// back into the result variable if we are not returning void. The
/// result variable may be the ExitValue, or the Value of the calling
/// CallInst if there was a previous stack frame. This method may
/// invalidate any ECStack iterators you have. This method also takes
/// care of switching to the normal destination BB, if we are returning
/// from an invoke.
///
void Interpreter::popStackAndReturnValueToCaller(Type *RetTy,
                                                 GenericValue Result) {
    leave();

    if (stack().empty()) {  // Finished main.  Put result into exit code...
        if (RetTy && !RetTy->isVoidTy()) {          // Nonvoid return type?
            ExitValue = Result;   // Capture the exit value of the program
        } else {
            memset(&ExitValue.Untyped, 0, sizeof(ExitValue.Untyped));
        }
    } else {
        CallSite CS = caller( SF() );
        // If we have a previous stack frame, and we have a previous call,
        // fill in the return value...
        if (Instruction *I = CS.getInstruction()) {
            // Save result...
            if (!CS.getType()->isVoidTy())
                SetValue(I, Result, SF());
            if (InvokeInst *II = dyn_cast<InvokeInst> (I))
                SwitchToNewBasicBlock (II->getNormalDest (), SF());
            SF().caller = -1;          // XXX? We returned from the call...
        }
    }
}

void Interpreter::visitReturnInst(ReturnInst &I) {
  Type *RetTy = Type::getVoidTy(I.getContext());
  GenericValue Result;

  // Save away the return value... (if we are not 'ret void')
  if (I.getNumOperands()) {
    RetTy  = I.getReturnValue()->getType();
    Result = getOperandValue(I.getReturnValue(), SF());
  }

  popStackAndReturnValueToCaller(RetTy, Result);
}

void Interpreter::visitUnwindInst(UnwindInst &I) {
  // Unwind stack
  Instruction *Inst;
  do {
      leave();
      if (stack().empty())
          report_fatal_error("Empty stack during unwind!");
      Inst = caller( SF() ).getInstruction();
  } while (!(Inst && isa<InvokeInst>(Inst)));

  // Return from invoke
  SF().caller = -1; // CallSite();

  // Go to exceptional destination BB of invoke instruction
  SwitchToNewBasicBlock(cast<InvokeInst>(Inst)->getUnwindDest(), SF());
}

void Interpreter::visitUnreachableInst(UnreachableInst &I) {
  report_fatal_error("Program executed an 'unreachable' instruction!");
}

void Interpreter::visitBranchInst(BranchInst &I) {
  BasicBlock *Dest;

  Dest = I.getSuccessor(0);          // Uncond branches have a fixed dest...
  if (!I.isUnconditional()) {
    Value *Cond = I.getCondition();
    if (getOperandValue(Cond, SF()).IntVal == 0) // If false cond...
      Dest = I.getSuccessor(1);
  }
  SwitchToNewBasicBlock(Dest, SF());
}

void Interpreter::visitSwitchInst(SwitchInst &I) {
    GenericValue CondVal = getOperandValue(I.getOperand(0), SF());
    Type *ElTy = I.getOperand(0)->getType();

    // Check to see if any of the cases match...
    BasicBlock *Dest = 0;
    for (unsigned i = 2, e = I.getNumOperands(); i != e; i += 2)
        if (executeICMP_EQ(CondVal, getOperandValue(I.getOperand(i), SF()), ElTy)
            .IntVal != 0) {
            Dest = cast<BasicBlock>(I.getOperand(i+1));
            break;
        }

    if (!Dest) Dest = I.getDefaultDest();   // No cases matched: use default
    SwitchToNewBasicBlock(Dest, SF());
}

void Interpreter::visitIndirectBrInst(IndirectBrInst &I) {
    void *Dest = GVTOP(getOperandValue(I.getAddress(), SF()));
    SwitchToNewBasicBlock((BasicBlock*)Dest, SF());
}


// SwitchToNewBasicBlock - This method is used to jump to a new basic block.
// This function handles the actual updating of block and instruction iterators
// as well as execution of all of the PHI nodes in the destination block.
//
// This method does this because all of the PHI nodes must be executed
// atomically, reading their inputs before any of the results are updated.  Not
// doing this can cause problems if the PHI nodes depend on other PHI nodes for
// their inputs.  If the input PHI node is updated before it is read, incorrect
// results can happen.  Thus we use a two phase approach.
//
void Interpreter::SwitchToNewBasicBlock(BasicBlock *Dest, ExecutionContext &SF){
    Location previous = location( SF );
    Location next( previous.function, Dest, Dest->begin() );
    setLocation( SF, next );

    if (!isa<PHINode>(next.insn)) return;  // Nothing fancy to do

    // Loop over all of the PHI nodes in the current block, reading their inputs.
    std::vector<GenericValue> ResultValues;

    for (; PHINode *PN = dyn_cast<PHINode>(next.insn); ++next.insn) {
        // Search for the value corresponding to this previous bb...
        int i = PN->getBasicBlockIndex(previous.block);
        assert(i != -1 && "PHINode doesn't contain entry for predecessor??");
        Value *IncomingValue = PN->getIncomingValue(i);

        // Save the incoming value for this PHI node...
        ResultValues.push_back(getOperandValue(IncomingValue, SF));
    }

    // Now loop over all of the PHI nodes setting their values...
    next.insn = next.block->begin();
    for (unsigned i = 0; isa<PHINode>(next.insn); ++next.insn, ++i) {
        PHINode *PN = cast<PHINode>(next.insn);
        SetValue(PN, ResultValues[i], SF);
    }
    setLocation( SF, next );
}

//===----------------------------------------------------------------------===//
//                     Memory Instruction Implementations
//===----------------------------------------------------------------------===//

void Interpreter::visitAllocaInst(AllocaInst &I) {
    Type *Ty = I.getType()->getElementType();  // Type to be allocated

    // Get the number of elements being allocated by the array...
    unsigned NumElements =
        getOperandValue(I.getOperand(0), SF()).IntVal.getZExtValue();

    unsigned TypeSize = (size_t)TD.getTypeAllocSize(Ty);

    // Avoid malloc-ing zero bytes, use max()...
    unsigned MemToAlloc = std::max(1U, NumElements * TypeSize);

    // Allocate enough memory to hold the type...
    Arena::Index Memory = arena.allocate(MemToAlloc);

    GenericValue Result;
    Result.PointerVal = reinterpret_cast< void * >( intptr_t( Memory ) );
    Result.IntVal = APInt(2, 0); // XXX, not very clean; marks an alloca for cloning by detach()
    assert(Result.PointerVal != 0 && "Null pointer returned by malloc!");
    SetValue(&I, Result, SF());

    if (I.getOpcode() == Instruction::Alloca)
        SF().allocas.push_back(Memory);
}

// getElementOffset - The workhorse for getelementptr.
//
GenericValue Interpreter::executeGEPOperation(Value *Ptr, gep_type_iterator I,
                                              gep_type_iterator E,
                                              ExecutionContext &SF) {
  assert(Ptr->getType()->isPointerTy() &&
         "Cannot getElementOffset of a nonpointer type!");

  uint64_t Total = 0;

  for (; I != E; ++I) {
    if (StructType *STy = dyn_cast<StructType>(*I)) {
      const StructLayout *SLO = TD.getStructLayout(STy);

      const ConstantInt *CPU = cast<ConstantInt>(I.getOperand());
      unsigned Index = unsigned(CPU->getZExtValue());

      Total += SLO->getElementOffset(Index);
    } else {
      const SequentialType *ST = cast<SequentialType>(*I);
      // Get the index number for the array... which must be long type...
      GenericValue IdxGV = getOperandValue(I.getOperand(), SF);

      int64_t Idx;
      unsigned BitWidth = 
        cast<IntegerType>(I.getOperand()->getType())->getBitWidth();
      if (BitWidth == 32)
        Idx = (int64_t)(int32_t)IdxGV.IntVal.getZExtValue();
      else {
        assert(BitWidth == 64 && "Invalid index type for getelementptr");
        Idx = (int64_t)IdxGV.IntVal.getZExtValue();
      }
      Total += TD.getTypeAllocSize(ST->getElementType())*Idx;
    }
  }

  GenericValue Result;
  Result.PointerVal = ((char*)getOperandValue(Ptr, SF).PointerVal) + Total;
  return Result;
}

void Interpreter::visitGetElementPtrInst(GetElementPtrInst &I) {
  SetValue(&I, executeGEPOperation(I.getPointerOperand(),
                                   gep_type_begin(I), gep_type_end(I), SF()), SF());
}

GenericValue *Interpreter::dereferencePointer(GenericValue GV) {
    if ( GVTOP(GV) == 0 ) {
        flags.null_dereference = true;
        return 0;
    }

    if ( intptr_t(GVTOP(GV)) - 0x100 < constGlobalmem.size() + globalmem.size()) {
        // global
        if ( intptr_t(GVTOP(GV)) - 0x100 < constGlobalmem.size() ) {
            // const global
            return (GenericValue *) &constGlobalmem[intptr_t(GVTOP(GV)) - 0x100];
        } else {
            // non-const global
            return (GenericValue *) &globalmem[intptr_t(GVTOP(GV)) - 0x100 - constGlobalmem.size()];
        }
    }

    if ( !arena.validate( intptr_t( GVTOP(GV) ) ) ) {
        flags.invalid_dereference = true;
        assert_eq( GVTOP(GV), (void*)0 );
        return 0;
    }

    return (GenericValue*) arena.translate(intptr_t(GVTOP(GV)));
}

void Interpreter::visitLoadInst(LoadInst &I) {
    GenericValue SRC = getOperandValue(I.getPointerOperand(), SF());
    GenericValue *Ptr;

    if ( !(Ptr = dereferencePointer( SRC )) )
        return;

    GenericValue Result;
    LoadValueFromMemory(Result, Ptr, I.getType());
    SetValue(&I, Result, SF());
}

void Interpreter::visitStoreInst(StoreInst &I) {
    GenericValue Val = getOperandValue(I.getOperand(0), SF());
    GenericValue SRC = getOperandValue(I.getPointerOperand(), SF());
    GenericValue *Ptr;

    if ( !(Ptr = dereferencePointer( SRC )) )
        return;

    StoreValueToMemory(Val, Ptr, I.getOperand(0)->getType());
}

void Interpreter::setInstruction( ExecutionContext &SF, BasicBlock::iterator i ) {
    Location loc = location( SF );
    loc.insn = i;
    setLocation( SF, loc );
}

//===----------------------------------------------------------------------===//
//                 Miscellaneous Instruction Implementations
//===----------------------------------------------------------------------===//

void Interpreter::visitFenceInst(FenceInst &I) {
    // do nothing for now
}

void Interpreter::visitCallSite(CallSite CS) {
    // Check to see if this is an intrinsic function call...
    Function *F = CS.getCalledFunction();
    if (F && F->isDeclaration())
        switch (F->getIntrinsicID()) {
            case Intrinsic::not_intrinsic:
                break;
            case Intrinsic::vastart: { // va_start
                GenericValue ArgIndex;
                ArgIndex.UIntPairVal.first = stack().size() - 1;
                ArgIndex.UIntPairVal.second = 0;
                SetValue(CS.getInstruction(), ArgIndex, SF());
                return;
            }
            // noops
            case Intrinsic::dbg_declare:
            case Intrinsic::dbg_value:
                return;
            case Intrinsic::trap:
                while (!stack().empty()) /* get us out */
                    leave();
                return;

            case Intrinsic::vaend:    // va_end is a noop for the interpreter
                return;
            case Intrinsic::vacopy:   // va_copy: dest = src
                SetValue(CS.getInstruction(), getOperandValue(*CS.arg_begin(), SF()), SF());
                return;
            default:
                // If it is an unknown intrinsic function, use the intrinsic lowering
                // class to transform it into hopefully tasty LLVM code.
                //
                // dbgs() << "FATAL: Can't lower:" << *CS.getInstruction() << "\n";
                assert_die(); /* TODO: the new locations need to be indexed */
                BasicBlock::iterator me(CS.getInstruction());
                BasicBlock *Parent = CS.getInstruction()->getParent();
                bool atBegin(Parent->begin() == me);
                if (!atBegin)
                    --me;
                IL->LowerIntrinsicCall(cast<CallInst>(CS.getInstruction()));

                // Restore the CurInst pointer to the first instruction newly inserted, if
                // any.
                if (atBegin) {
                    setInstruction( SF(), Parent->begin() );
                } else {
                    BasicBlock::iterator me_next = me;
                    ++ me_next;
                    setInstruction( SF(), me_next );
                }
                return;
        }

    Location loc = location( SF() );
    loc.insn = CS.getInstruction();
    SF().caller = locationIndex.left( loc );
    std::vector<GenericValue> ArgVals;
    const unsigned NumArgs = CS.arg_size();
    ArgVals.reserve(NumArgs);
    uint16_t pNum = 1;
    for (CallSite::arg_iterator i = CS.arg_begin(),
                                e = CS.arg_end(); i != e; ++i, ++pNum) {
        Value *V = *i;
        ArgVals.push_back(getOperandValue(V, SF()));
    }

    // To handle indirect calls, we must get the pointer value from the argument
    // and treat it as a function pointer.
    GenericValue SRC = getOperandValue(CS.getCalledValue(), SF());
    Function *fun = functionIndex.right(int(intptr_t(GVTOP(SRC))));
    callFunction(fun, ArgVals);
}

void Interpreter::visitShl(BinaryOperator &I) {
    assert_eq( _alternative, 0 );
    GenericValue Src1 = getOperandValue(I.getOperand(0), SF());
    GenericValue Src2 = getOperandValue(I.getOperand(1), SF());
    GenericValue Dest;
    if (Src2.IntVal.getZExtValue() < Src1.IntVal.getBitWidth())
        Dest.IntVal = Src1.IntVal.shl(Src2.IntVal.getZExtValue());
    else
        Dest.IntVal = Src1.IntVal;

    SetValue(&I, Dest, SF());
}

void Interpreter::visitLShr(BinaryOperator &I) {
    assert_eq( _alternative, 0 );
    GenericValue Src1 = getOperandValue(I.getOperand(0), SF());
    GenericValue Src2 = getOperandValue(I.getOperand(1), SF());
    GenericValue Dest;
    if (Src2.IntVal.getZExtValue() < Src1.IntVal.getBitWidth())
        Dest.IntVal = Src1.IntVal.lshr(Src2.IntVal.getZExtValue());
    else
        Dest.IntVal = Src1.IntVal;

    SetValue(&I, Dest, SF());
}

void Interpreter::visitAShr(BinaryOperator &I) {
    assert_eq( _alternative, 0 );
    GenericValue Src1 = getOperandValue(I.getOperand(0), SF());
    GenericValue Src2 = getOperandValue(I.getOperand(1), SF());
    GenericValue Dest;
    if (Src2.IntVal.getZExtValue() < Src1.IntVal.getBitWidth())
        Dest.IntVal = Src1.IntVal.ashr(Src2.IntVal.getZExtValue());
    else
        Dest.IntVal = Src1.IntVal;

    SetValue(&I, Dest, SF());
}

GenericValue Interpreter::executeTruncInst(Value *SrcVal, Type *DstTy,
                                           ExecutionContext &SF) {
    assert_eq( _alternative, 0 );
  GenericValue Dest, Src = getOperandValue(SrcVal, SF);
  const IntegerType *DITy = cast<IntegerType>(DstTy);
  unsigned DBitWidth = DITy->getBitWidth();
  Dest.IntVal = Src.IntVal.trunc(DBitWidth);
  return Dest;
}

GenericValue Interpreter::executeSExtInst(Value *SrcVal, Type *DstTy,
                                          ExecutionContext &SF) {
    assert_eq( _alternative, 0 );
  GenericValue Dest, Src = getOperandValue(SrcVal, SF);
  const IntegerType *DITy = cast<IntegerType>(DstTy);
  unsigned DBitWidth = DITy->getBitWidth();
  Dest.IntVal = Src.IntVal.sext(DBitWidth);
  return Dest;
}

GenericValue Interpreter::executeZExtInst(Value *SrcVal, Type *DstTy,
                                          ExecutionContext &SF) {
    assert_eq( _alternative, 0 );
  GenericValue Dest, Src = getOperandValue(SrcVal, SF);
  const IntegerType *DITy = cast<IntegerType>(DstTy);
  unsigned DBitWidth = DITy->getBitWidth();
  Dest.IntVal = Src.IntVal.zext(DBitWidth);
  return Dest;
}

GenericValue Interpreter::executeFPTruncInst(Value *SrcVal, Type *DstTy,
                                             ExecutionContext &SF) {
    assert_eq( _alternative, 0 );
  GenericValue Dest, Src = getOperandValue(SrcVal, SF);
  assert(SrcVal->getType()->isDoubleTy() && DstTy->isFloatTy() &&
         "Invalid FPTrunc instruction");
  Dest.FloatVal = (float) Src.DoubleVal;
  return Dest;
}

GenericValue Interpreter::executeFPExtInst(Value *SrcVal, Type *DstTy,
                                           ExecutionContext &SF) {
    assert_eq( _alternative, 0 );
  GenericValue Dest, Src = getOperandValue(SrcVal, SF);
  assert(SrcVal->getType()->isFloatTy() && DstTy->isDoubleTy() &&
         "Invalid FPTrunc instruction");
  Dest.DoubleVal = (double) Src.FloatVal;
  return Dest;
}

GenericValue Interpreter::executeFPToUIInst(Value *SrcVal, Type *DstTy,
                                            ExecutionContext &SF) {
    assert_eq( _alternative, 0 );
  Type *SrcTy = SrcVal->getType();
  uint32_t DBitWidth = cast<IntegerType>(DstTy)->getBitWidth();
  GenericValue Dest, Src = getOperandValue(SrcVal, SF);
  assert(SrcTy->isFloatingPointTy() && "Invalid FPToUI instruction");

  if (SrcTy->getTypeID() == Type::FloatTyID)
    Dest.IntVal = APIntOps::RoundFloatToAPInt(Src.FloatVal, DBitWidth);
  else
    Dest.IntVal = APIntOps::RoundDoubleToAPInt(Src.DoubleVal, DBitWidth);
  return Dest;
}

GenericValue Interpreter::executeFPToSIInst(Value *SrcVal, Type *DstTy,
                                            ExecutionContext &SF) {
    assert_eq( _alternative, 0 );
  Type *SrcTy = SrcVal->getType();
  uint32_t DBitWidth = cast<IntegerType>(DstTy)->getBitWidth();
  GenericValue Dest, Src = getOperandValue(SrcVal, SF);
  assert(SrcTy->isFloatingPointTy() && "Invalid FPToSI instruction");

  if (SrcTy->getTypeID() == Type::FloatTyID)
    Dest.IntVal = APIntOps::RoundFloatToAPInt(Src.FloatVal, DBitWidth);
  else
    Dest.IntVal = APIntOps::RoundDoubleToAPInt(Src.DoubleVal, DBitWidth);
  return Dest;
}

GenericValue Interpreter::executeUIToFPInst(Value *SrcVal, Type *DstTy,
                                            ExecutionContext &SF) {
    assert_eq( _alternative, 0 );
  GenericValue Dest, Src = getOperandValue(SrcVal, SF);
  assert(DstTy->isFloatingPointTy() && "Invalid UIToFP instruction");

  if (DstTy->getTypeID() == Type::FloatTyID)
    Dest.FloatVal = APIntOps::RoundAPIntToFloat(Src.IntVal);
  else
    Dest.DoubleVal = APIntOps::RoundAPIntToDouble(Src.IntVal);
  return Dest;
}

GenericValue Interpreter::executeSIToFPInst(Value *SrcVal, Type *DstTy,
                                            ExecutionContext &SF) {
    assert_eq( _alternative, 0 );
  GenericValue Dest, Src = getOperandValue(SrcVal, SF);
  assert(DstTy->isFloatingPointTy() && "Invalid SIToFP instruction");

  if (DstTy->getTypeID() == Type::FloatTyID)
    Dest.FloatVal = APIntOps::RoundSignedAPIntToFloat(Src.IntVal);
  else
    Dest.DoubleVal = APIntOps::RoundSignedAPIntToDouble(Src.IntVal);
  return Dest;

}

GenericValue Interpreter::executePtrToIntInst(Value *SrcVal, Type *DstTy,
                                              ExecutionContext &SF) {
    assert_eq( _alternative, 0 );
  uint32_t DBitWidth = cast<IntegerType>(DstTy)->getBitWidth();
  GenericValue Dest, Src = getOperandValue(SrcVal, SF);
  assert(SrcVal->getType()->isPointerTy() && "Invalid PtrToInt instruction");

  Dest.IntVal = APInt(DBitWidth, (intptr_t) Src.PointerVal);
  return Dest;
}

GenericValue Interpreter::executeIntToPtrInst(Value *SrcVal, Type *DstTy,
                                              ExecutionContext &SF) {
    assert_eq( _alternative, 0 );
  GenericValue Dest, Src = getOperandValue(SrcVal, SF);
  assert(DstTy->isPointerTy() && "Invalid PtrToInt instruction");

  uint32_t PtrSize = TD.getPointerSizeInBits();
  if (PtrSize != Src.IntVal.getBitWidth())
    Src.IntVal = Src.IntVal.zextOrTrunc(PtrSize);

  Dest.PointerVal = PointerTy(intptr_t(Src.IntVal.getZExtValue()));
  return Dest;
}

GenericValue Interpreter::executeBitCastInst(Value *SrcVal, Type *DstTy,
                                             ExecutionContext &SF) {
    assert_eq( _alternative, 0 );
  
  Type *SrcTy = SrcVal->getType();
  GenericValue Dest, Src = getOperandValue(SrcVal, SF);
  if (DstTy->isPointerTy()) {
    assert(SrcTy->isPointerTy() && "Invalid BitCast");
    Dest.PointerVal = Src.PointerVal;
  } else if (DstTy->isIntegerTy()) {
    if (SrcTy->isFloatTy()) {
      Dest.IntVal = APInt::floatToBits(Src.FloatVal);
    } else if (SrcTy->isDoubleTy()) {
      Dest.IntVal = APInt::doubleToBits(Src.DoubleVal);
    } else if (SrcTy->isIntegerTy()) {
      Dest.IntVal = Src.IntVal;
    } else 
      llvm_unreachable("Invalid BitCast");
  } else if (DstTy->isFloatTy()) {
    if (SrcTy->isIntegerTy())
      Dest.FloatVal = Src.IntVal.bitsToFloat();
    else
      Dest.FloatVal = Src.FloatVal;
  } else if (DstTy->isDoubleTy()) {
    if (SrcTy->isIntegerTy())
      Dest.DoubleVal = Src.IntVal.bitsToDouble();
    else
      Dest.DoubleVal = Src.DoubleVal;
  } else
    llvm_unreachable("Invalid Bitcast");

  return Dest;
}

void Interpreter::visitTruncInst(TruncInst &I) {
    SetValue(&I, executeTruncInst(I.getOperand(0), I.getType(), SF()), SF());
}

void Interpreter::visitSExtInst(SExtInst &I) {
    SetValue(&I, executeSExtInst(I.getOperand(0), I.getType(), SF()), SF());
}

void Interpreter::visitZExtInst(ZExtInst &I) {
    SetValue(&I, executeZExtInst(I.getOperand(0), I.getType(), SF()), SF());
}

void Interpreter::visitFPTruncInst(FPTruncInst &I) {
    SetValue(&I, executeFPTruncInst(I.getOperand(0), I.getType(), SF()), SF());
}

void Interpreter::visitFPExtInst(FPExtInst &I) {
    SetValue(&I, executeFPExtInst(I.getOperand(0), I.getType(), SF()), SF());
}

void Interpreter::visitUIToFPInst(UIToFPInst &I) {
    SetValue(&I, executeUIToFPInst(I.getOperand(0), I.getType(), SF()), SF());
}

void Interpreter::visitSIToFPInst(SIToFPInst &I) {
    SetValue(&I, executeSIToFPInst(I.getOperand(0), I.getType(), SF()), SF());
}

void Interpreter::visitFPToUIInst(FPToUIInst &I) {
    SetValue(&I, executeFPToUIInst(I.getOperand(0), I.getType(), SF()), SF());
}

void Interpreter::visitFPToSIInst(FPToSIInst &I) {
    SetValue(&I, executeFPToSIInst(I.getOperand(0), I.getType(), SF()), SF());
}

void Interpreter::visitPtrToIntInst(PtrToIntInst &I) {
    SetValue(&I, executePtrToIntInst(I.getOperand(0), I.getType(), SF()), SF());
}

void Interpreter::visitIntToPtrInst(IntToPtrInst &I) {
    SetValue(&I, executeIntToPtrInst(I.getOperand(0), I.getType(), SF()), SF());
}

void Interpreter::visitBitCastInst(BitCastInst &I) {
    SetValue(&I, executeBitCastInst(I.getOperand(0), I.getType(), SF()), SF());
}

#define IMPLEMENT_VAARG(TY) \
   case Type::TY##TyID: Dest.TY##Val = Src.TY##Val; break

void Interpreter::visitVAArgInst(VAArgInst &I) {
    // Get the incoming valist parameter.  LLI treats the valist as a
    // (ec-stack-depth var-arg-index) pair.
    GenericValue VAList = getOperandValue(I.getOperand(0), SF());
    GenericValue Dest;
    GenericValue Src = SFat(VAList.UIntPairVal.first)
                       .varArgs[VAList.UIntPairVal.second];
    Type *Ty = I.getType();
    switch (Ty->getTypeID()) {
        case Type::IntegerTyID: Dest.IntVal = Src.IntVal;
            IMPLEMENT_VAARG(Pointer);
            IMPLEMENT_VAARG(Float);
            IMPLEMENT_VAARG(Double);
        default:
            assert_die();
    }

    // Set the Value of this Instruction.
    SetValue(&I, Dest, SF());

    // Move the pointer to the next vararg.
    ++VAList.UIntPairVal.second;
}

GenericValue Interpreter::getOperandValue(Value *V, ExecutionContext &SF) {
    GlobalVariable *GV;
    if (GV = dyn_cast<GlobalVariable>(V)) {
        std::map< const GlobalVariable *, int >::const_iterator i = globals.find( GV );
        assert( i != globals.end() );
        return PTOGV((void*) i->second);
    } else if (Constant *CPV = dyn_cast<Constant>(V)) {
        return getConstantValue(CPV);
    } else {
        ExecutionContext::Values::const_iterator i = SF.values.find( valueIndex.left( V ) );
        assert( i != SF.values.end() );
        return i->second;
    }
}

//===----------------------------------------------------------------------===//
//                        Dispatch and Execution Code
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// callFunction - Execute the specified function...
//
void Interpreter::callFunction(Function *F,
                               const std::vector<GenericValue> &ArgVals) {
    assert((stack().empty() || caller( SF() ).getInstruction() == 0 ||
            caller( SF() ).arg_size() == ArgVals.size()) &&
           "Incorrect number of arguments passed into function call!");
    // Make a new stack frame... and fill it in.
    enter();

    // Special handling for external functions.
    if (F->isDeclaration()) {
        /* This traps into the "externals": functions that may be provided by
         * our own runtime (these may be nondeterministic), or, possibly (TODO)
         * into external, native library code  */
        GenericValue Result = callExternalFunction (F, ArgVals);
        if ( stack().size() >= 2 && SFat( -2 ).pc == SFat( -2 ).lastpc )
            leave(); // this call was restarted, so do not alter anything
        else
            // Simulate a 'ret' instruction of the appropriate type.
            popStackAndReturnValueToCaller(F->getReturnType(), Result);

        return;
    } else { // a normal "call" instruction is deterministic
        assert_eq( _alternative, 0 );
    }

    Location next( F, F->begin(), F->begin()->begin() );
    setLocation( SF(), next );

    // Run through the function arguments and initialize their values...
    assert((ArgVals.size() == F->arg_size() ||
            (ArgVals.size() > F->arg_size() && F->getFunctionType()->isVarArg()))&&
           "Invalid number of values passed to function invocation!");

    // Handle non-varargs arguments...
    unsigned i = 0;
    for (Function::arg_iterator AI = F->arg_begin(), E = F->arg_end(); 
         AI != E; ++AI, ++i)
        SetValue(AI, ArgVals[i], SF());

    // Handle varargs arguments...
    SF().varArgs.assign(ArgVals.begin()+i, ArgVals.end());
}

bool Interpreter::done( int ctx ) {
    return stack( ctx ).empty();
}

Instruction &Interpreter::nextInstruction() {
    return *locationIndex.right( SF().pc ).insn;
}

void Interpreter::step( int ctx, int alternative ) {
    _context = ctx;
    _alternative = alternative;
    flags.assert = false; // reset assert flag
    flags.ap = 0; // XXX a wholesale reset...

    SF().lastpc = SF().pc;

    Location loc = location( SF() );
    Instruction &I = *loc.insn++;
    setLocation( SF(), loc );

    std::string descr;
    raw_string_ostream descr_stream( descr );
    descr_stream << I;

    const LLVMContext &xx = I.getContext();
    const DebugLoc &dloc = I.getDebugLoc();
    DILocation des( dloc.getAsMDNode( xx ) );
    /* std::cerr << "executing " << descr << "; at ";
    if ( des.getLineNumber() )
        std::cerr << std::string(des.getFilename()) << ":" << des.getLineNumber();
        std::cerr << std::endl; */

    visit( I );

    // remove the context if we are done with it
    if ( done( ctx ) ) {
        assert( (threads.begin() + ctx)->stack.empty() );
        threads.erase( threads.begin() + ctx );
    }
}

void Interpreter::run() {
    while ( !done() ) {
        step();
    }
}
