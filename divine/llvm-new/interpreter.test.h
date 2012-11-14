#include <divine/llvm-new/interpreter.h>
#include "llvm/LLVMContext.h"
#include "llvm/Support/IRBuilder.h"

using namespace llvm;
namespace dlvm = divine::llvm2;

struct TestLLVM {
    LLVMContext &ctx;
    IRBuilder<> builder;

    TestLLVM() : ctx( getGlobalContext() ), builder( ctx ) {}

    Function *_main() {
        Module *m = new Module( "code1", ctx );
        FunctionType *mainT = FunctionType::get(Type::getInt64Ty(ctx), std::vector<Type *>(), false);
        Function *main = Function::Create(mainT, Function::ExternalLinkage, "test_main", m);
        BasicBlock *BB = BasicBlock::Create(ctx, "entry", main);
        builder.SetInsertPoint( BB );
        return main;
    }

    Function *code1() {
        Function *main = _main();
        Value *result = ConstantInt::get(ctx, APInt(64, 1));
        builder.CreateRet( result );
        return main;
    }

    Function *code2() {
        Function *f = _main();
        builder.CreateBr( &*(f->begin()) );
        return f;
    }

    Test initial()
    {
        divine::Allocator a;
        Function *main = code1();
        dlvm::Interpreter i( a, main->getParent() );
        i.initial( main );
    }

    Test successor1()
    {
        divine::Allocator a;
        Function *main = code1();
        dlvm::Interpreter i( a, main->getParent() );
        divine::Blob ini = i.initial( main ), fin;
        i.rewind( ini );
        i.run( 0, [&]( divine::Blob b ) {
                assert( !fin.valid() ); // only one allowed
                fin = b;
            });
        assert( fin.valid() );
        // 2x flags, thread count, thread 1( stack, pagetable, memory size )
        assert_eq( wibble::str::fmt( fin ), "[ 0, 0, 1, 0, 0, 0 ]" );
    }

    Test successor2()
    {
        divine::Allocator a;
        Function *main = code2();
        dlvm::Interpreter i( a, main->getParent() );
        divine::Blob ini = i.initial( main ), fin;
        i.rewind( ini );
        i.run( 0, [&]( divine::Blob b ) {
                assert( !fin.valid() ); // only one allowed
                fin = b;
            });
        assert( fin.valid() );
        // 2x flags, thread count, thread 1( stack size ( pc ), pagetable, memory size )
        assert_eq( wibble::str::fmt( fin ), "[ 0, 0, 1, 1, 1, 0, 0, 0 ]" );
    }
};
