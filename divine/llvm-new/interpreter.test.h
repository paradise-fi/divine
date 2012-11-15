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
        FunctionType *mainT = FunctionType::get(Type::getInt32Ty(ctx), std::vector<Type *>(), false);
        Function *main = Function::Create(mainT, Function::ExternalLinkage, "testf", m);
        BasicBlock *BB = BasicBlock::Create(ctx, "entry", main);
        builder.SetInsertPoint( BB );
        return main;
    }

    Function *code1() {
        Function *main = _main();
        Value *result = ConstantInt::get(ctx, APInt(32, 1));
        builder.CreateRet( result );
        return main;
    }

    Function *code2() {
        Function *f = _main();
        builder.CreateBr( &*(f->begin()) );
        return f;
    }

    Function *code3() {
        Function *f = _main();
        Value *a = ConstantInt::get(ctx, APInt(32, 1)),
              *b = ConstantInt::get(ctx, APInt(32, 2));
        Value *result = builder.Insert( BinaryOperator::Create( Instruction::Add, a, b ), "meh" );
        builder.CreateBr( &*(f->begin()) );
        builder.CreateRet( result );
        return f;
    }

    divine::Blob _ith( Function *f, int step ) {
        divine::Allocator a;
        dlvm::Interpreter interpreter( a, f->getParent() );
        divine::Blob ini = interpreter.initial( f ), fin;
        interpreter.rewind( ini );
        fin = ini;

        for ( int i = 0; i < step; ++i ) {
            fin = divine::Blob();
            interpreter.run( 0, [&]( divine::Blob b ) {
                    assert( !fin.valid() ); // only one allowed
                    fin = b;
                });
            assert( fin.valid() );
        }

        return fin;
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
        assert_eq( wibble::str::fmt( _ith( code1(), 1 ) ),
                   "[ 0, 0, 1, 0, 0, 0 ]" );
    }

    Test successor2()
    {
        assert_eq( wibble::str::fmt( _ith( code2(), 1 ) ),
                   "[ 0, 0, 1, 1, 1, 0, 0, 0 ]" );
    }

    Test successor3()
    {
        assert_eq( wibble::str::fmt( _ith( code3(), 1 ) ),
                   "[ 0, 0, 1, 1, 1, 3, 0, 0, 0 ]" );
        assert_eq( wibble::str::fmt( _ith( code3(), 2 ) ),
                   "[ 0, 0, 1, 1, 1, 3, 0, 0, 0 ]" );
    }

    Test describe1()
    {
        Function *f = code2();
        divine::Allocator a;
        dlvm::Interpreter interpreter( a, f->getParent() );
        divine::Blob b = _ith( code2(), 1 );
        interpreter.rewind( b );
        assert_eq( "0: <testf> [ br label %entry ] []\n", interpreter.describe() );
    }

    Test describe2()
    {
        Function *f = code2();
        divine::Allocator a;
        dlvm::Interpreter interpreter( a, f->getParent() );
        divine::Blob b = _ith( code2(), 1 );
        interpreter.rewind( b );
        interpreter.new_thread( f );
        assert_eq( "0: <testf> [ br label %entry ] []\n"
                   "1: <testf> [ br label %entry ] []\n", interpreter.describe() );
    }

    Test describe3()
    {
        Function *f = code3();
        divine::Allocator a;
        dlvm::Interpreter interpreter( a, f->getParent() );
        divine::Blob b = interpreter.initial( f );
        assert_eq( "0: <testf> [ %meh = add i32 1, 2 ] [ meh = 0 ]\n", interpreter.describe() );

        b = _ith( code3(), 1 );
        interpreter.rewind( b );
        assert_eq( "0: <testf> [ %meh = add i32 1, 2 ] [ meh = 3 ]\n", interpreter.describe() );
    }

    Test idempotency()
    {
        Function *f = code2();
        divine::Allocator a;
        dlvm::Interpreter interpreter( a, f->getParent() );
        divine::Blob b1 = interpreter.initial( f ), b2;
        interpreter.rewind( b1 );
        b2 = interpreter.state.snapshot();
        assert( b1 == b2 );
    }
};
