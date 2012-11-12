#include <divine/llvm-new/interpreter.h>
#include "llvm/LLVMContext.h"
#include "llvm/Support/IRBuilder.h"

using namespace llvm;
namespace dlvm = divine::llvm2;

struct TestLLVM {

    Function *code1() {
        LLVMContext &ctx = getGlobalContext();
        IRBuilder<> builder( ctx );
        Module *m = new Module( "code1", ctx );
        FunctionType *mainT = FunctionType::get(Type::getInt64Ty(ctx), std::vector<Type *>(), false);
        Function *main = Function::Create(mainT, Function::ExternalLinkage, "test_main", m);
        BasicBlock *BB = BasicBlock::Create(ctx, "entry", main);

        builder.SetInsertPoint( BB );
        Value *result = ConstantInt::get(ctx, APInt(64, 1));
        builder.CreateRet( result );

        return main;
    }

    Test initial()
    {
        divine::Allocator a;
        Function *main = code1();
        dlvm::Interpreter i( a, main->getParent() );
        i.initial( main );
    }
};
