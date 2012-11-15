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
        Module *m = new Module( "testm", ctx );
        FunctionType *mainT = FunctionType::get(Type::getInt32Ty(ctx), std::vector<Type *>(), false);
        Function *main = Function::Create(mainT, Function::ExternalLinkage, "testf", m);
        BasicBlock *BB = BasicBlock::Create(ctx, "entry", main);
        builder.SetInsertPoint( BB );
        return main;
    }

    Function *code_ret() {
        Function *main = _main();
        Value *result = ConstantInt::get(ctx, APInt(32, 1));
        builder.CreateRet( result );
        return main;
    }

    Function *code_loop() {
        Function *f = _main();
        builder.CreateBr( &*(f->begin()) );
        return f;
    }

    Function *code_add() {
        Function *f = _main();
        Value *a = ConstantInt::get(ctx, APInt(32, 1)),
              *b = ConstantInt::get(ctx, APInt(32, 2));
        Value *result = builder.Insert( BinaryOperator::Create( Instruction::Add, a, b ), "meh" );
        builder.CreateBr( &*(f->begin()) );
        builder.CreateRet( result );
        return f;
    }

    Function *code_call() {
        Function *f = _main();
        FunctionType *helperT = FunctionType::get(Type::getInt32Ty(ctx), std::vector<Type *>(), false);
        Function *helper = Function::Create(helperT, Function::ExternalLinkage, "helper", f->getParent());
        BasicBlock *BB = BasicBlock::Create(ctx, "entry", helper);
        builder.CreateCall( helper );
        builder.CreateBr( &*(f->begin()) );
        builder.SetInsertPoint( BB );
        builder.CreateBr( &*(helper->begin()) );
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

    std::string _descr( Function *f, divine::Blob b ) {
        divine::Allocator a;
        dlvm::Interpreter interpreter( a, f->getParent() );
        interpreter.rewind( b );
        return interpreter.describe();
    }

    Test initial()
    {
        divine::Allocator a;
        Function *main = code_ret();
        dlvm::Interpreter i( a, main->getParent() );
        i.initial( main );
    }

    Test successor1()
    {
        assert_eq( wibble::str::fmt( _ith( code_ret(), 1 ) ),
                   "[ 0, 0, 1, 0, 0, 0 ]" );
    }

    Test successor2()
    {
        assert_eq( wibble::str::fmt( _ith( code_loop(), 1 ) ),
                   "[ 0, 0, 1, 1, 1, 0, 0, 0 ]" );
    }

    Test successor3()
    {
        assert_eq( wibble::str::fmt( _ith( code_add(), 1 ) ),
                   "[ 0, 0, 1, 1, 1, 3, 0, 0, 0 ]" );
        assert_eq( wibble::str::fmt( _ith( code_add(), 2 ) ),
                   "[ 0, 0, 1, 1, 1, 3, 0, 0, 0 ]" );
    }

    Test describe1()
    {
        divine::Blob b = _ith( code_loop(), 1 );
        assert_eq( _descr( code_loop(), b ),
                   "0: <testf> [ br label %entry ] []\n" );
    }

    Test describe2()
    {
        Function *f = code_loop();
        divine::Allocator a;
        dlvm::Interpreter interpreter( a, f->getParent() );
        divine::Blob b = _ith( code_loop(), 1 );
        interpreter.rewind( b );
        interpreter.new_thread( f );
        assert_eq( "0: <testf> [ br label %entry ] []\n"
                   "1: <testf> [ br label %entry ] []\n", interpreter.describe() );
    }

    Test describe3()
    {
        divine::Blob b = _ith( code_add(), 0 );
        assert_eq( _descr( code_add(), b ),
                   "0: <testf> [ %meh = add i32 1, 2 ] [ meh = 0 ]\n" );

        b = _ith( code_add(), 1 );
        assert_eq( _descr( code_add(), b ),
                   "0: <testf> [ %meh = add i32 1, 2 ] [ meh = 3 ]\n" );
    }

    Test describe4()
    {
        divine::Blob b = _ith( code_call(), 0 );
        assert_eq( _descr( code_call(), b ),
                   "0: <testf> [ %0 = call i32 @helper() ] []\n" );
        b = _ith( code_call(), 1 );
        assert_eq( _descr( code_call(), b ),
                   "0: <helper> [ br label %entry ] []\n" );
    }

    Test idempotency()
    {
        Function *f = code_loop();
        divine::Allocator a;
        dlvm::Interpreter interpreter( a, f->getParent() );
        divine::Blob b1 = interpreter.initial( f ), b2;
        interpreter.rewind( b1 );
        b2 = interpreter.state.snapshot();
        assert( b1 == b2 );
    }
};
