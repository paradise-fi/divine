// -*- C++ -*-
#include <divine/llvm/interpreter.h>
#include <divine/graph/graph.h> // allocator :-(
#include <divine/llvm/wrap/LLVMContext.h>

#include <llvm/Config/config.h>
#if ( LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR < 2 )
#  include <llvm/Support/IRBuilder.h>
#else
#  include <divine/llvm/wrap/IRBuilder.h>
#endif

using namespace llvm;
namespace dlvm = divine::llvm;

struct TestLLVM {
    LLVMContext &ctx;
    IRBuilder<> builder;
    divine::Pool pool;
    std::shared_ptr< Module > module;
    using Interpreter = dlvm::Interpreter< dlvm::machine::NoHeapMeta, divine::graph::NoLabel >;

    TestLLVM() : ctx( getGlobalContext() ), builder( ctx ) {}

    std::shared_ptr< dlvm::BitCode > bitcode() {
        return std::make_shared< dlvm::BitCode >( module );
    }

    Function *_function( Module *m = NULL, const char *name = "testf", int pcount = 0 ) {
        if ( !m ) {
            module = std::make_shared< Module >( "testm", ctx );
            m = module.get();
        }
        std::vector< Type * > args;
        for ( int i = 0; i < pcount; ++ i )
            args.push_back( Type::getInt32Ty( ctx ) );
        FunctionType *t = FunctionType::get(Type::getInt32Ty(ctx), args, false);
        Function *f = Function::Create(t, Function::ExternalLinkage, name, m);
        BasicBlock *BB = BasicBlock::Create(ctx, "entry", f);
        builder.SetInsertPoint( BB );
        return f;
    }

    Function *code_ret() {
        Function *main = _function();
        Value *result = ConstantInt::get(ctx, APInt(32, 1));
        builder.CreateRet( result );
        return main;
    }

    Function *code_loop() {
        Function *f = _function();
        builder.CreateBr( &*(f->begin()) );
        return f;
    }

    Function *code_add() {
        Function *f = _function();
        Value *a = ConstantInt::get(ctx, APInt(32, 1)),
              *b = ConstantInt::get(ctx, APInt(32, 2));
        Value *result = builder.Insert( BinaryOperator::Create( Instruction::Add, a, b ), "meh" );
        builder.CreateBr( &*(f->begin()) );
        builder.CreateRet( result );
        return f;
    }

    Function *code_mem() {
        Function *f = _function();
        auto *mem = builder.CreateAlloca(Type::getInt32Ty(ctx), 0, "foo");
        builder.CreateStore(ConstantInt::get(ctx, APInt(32, 33)), mem);
        BasicBlock *BB = BasicBlock::Create(ctx, "tail", f);
        builder.SetInsertPoint( BB );
        builder.CreateBr( BB );
        return f;
    }

    Function *code_call() {
        Function *helper = _function( NULL, "helper", 0 );
        builder.CreateBr( &*(helper->begin()) );
        Function *f = _function( helper->getParent() );
        builder.CreateCall( helper );
        return f;
    }

    Function *code_callarg() {
        Function *helper = _function( NULL, "helper", 1 );
        builder.Insert( BinaryOperator::Create( Instruction::Add,
                                                helper->arg_begin(),
                                                helper->arg_begin() ), "meh" );
        builder.CreateBr( &*(helper->begin()) );
        Function *f = _function( helper->getParent() );
        builder.CreateCall( helper, ConstantInt::get(ctx, APInt(32, 7)) );
        return f;
    }

    Function *code_callret() {
        Function *helper = _function( NULL, "helper", 0 );
        builder.CreateRet( ConstantInt::get(ctx, APInt(32, 42)) );
        Function *f = _function( helper->getParent() );
        builder.CreateCall( helper, "meh" );
        builder.CreateBr( &*(f->begin()) );
        return f;
    }

    divine::Blob _ith( Function *f, int step ) {
        Interpreter interpreter( pool, 0, bitcode() );
        divine::Blob ini = interpreter.initial( f ), fin;
        fin = ini;

        for ( int i = 0; i < step; ++i ) {
            fin = divine::Blob();
            interpreter.run( ini, [&]( divine::Blob b, divine::graph::NoLabel ) {
                    assert( !pool.valid( fin ) ); // only one allowed
                    fin = b;
                });
            ini = fin;
            assert( pool.valid( fin ) );
        }

        return fin;
    }

    std::string _descr( Function *f, divine::Blob b ) {
        Interpreter interpreter( pool, 0, bitcode() );
        interpreter.rewind( b );
        return interpreter.describe();
    }

    Test initial()
    {
        Function *main = code_ret();
        Interpreter i( pool, 0, bitcode() );
        i.initial( main );
    }

    Test successor1()
    {
        assert_eq( fmtblob( pool, _ith( code_ret(), 1 ) ),
                   "[ 0, 0, 0, 0, 0 ]" );
    }

    Test successor2()
    {
        assert_eq( fmtblob( pool, _ith( code_loop(), 1 ) ),
                   "[ 0, 0, 0, 0, 1, 1, 2147745792 ]" );
    }

    Test successor3()
    {
        assert_eq( fmtblob( pool, _ith( code_add(), 2 ) ),
                   "[ 0, 0, 0, 0, 1, 1, 2147745792, 3, 1 ]" );
        assert_eq( fmtblob( pool, _ith( code_add(), 4 ) ),
                   "[ 0, 0, 0, 0, 1, 1, 2147745792, 3, 1 ]" );
    }

    Test describe1()
    {
        divine::Blob b = _ith( code_loop(), 1 );
        assert_eq( _descr( code_loop(), b ),
                   "thread 0:\n  #1: <testf> << br label %entry >> []\n" );
    }

    Test describe2()
    {
        Function *f = code_loop();
        Interpreter interpreter( pool, 0, bitcode() );
        divine::Blob b = _ith( code_loop(), 1 );
        interpreter.rewind( b );
        interpreter.new_thread( f );
        assert_eq( "thread 0:\n  #1: <testf> << br label %entry >> []\n"
                   "thread 1:\n  #1: <testf> << br label %entry >> []\n", interpreter.describe() );
    }

    Test describe3()
    {
        divine::Blob b = _ith( code_add(), 0 );
        assert_eq( _descr( code_add(), b ),
                   "thread 0:\n  #1: <testf> << %meh = add i32 1, 2 >> [ meh = ? ]\n" );

        b = _ith( code_add(), 2 );
        assert_eq( _descr( code_add(), b ),
                   "thread 0:\n  #1: <testf> << %meh = add i32 1, 2 >> [ meh = 3 ]\n" );
    }

    Test describe4()
    {
        divine::Blob b = _ith( code_call(), 0 );
        assert_eq( _descr( code_call(), b ),
                   "thread 0:\n  #1: <testf> << %0 = call i32 @helper() >> []\n" );
        b = _ith( code_call(), 1 );
        assert_eq( _descr( code_call(), b ),
                   "thread 0:\n"
                   "  #1: <testf> << %0 = call i32 @helper() >> []\n"
                   "  #2: <helper> << br label %entry >> []\n" );
    }

    Test describe5()
    {
        divine::Blob b = _ith( code_callarg(), 0 );
        assert_eq( _descr( code_callarg(), b ),
                   "thread 0:\n  #1: <testf> << %0 = call i32 @helper(i32 7) >> []\n" );
        b = _ith( code_callarg(), 3 );
        assert_eq( _descr( code_callarg(), b ),
                   "thread 0:\n"
                   "  #1: <testf> << %0 = call i32 @helper(i32 7) >> []\n"
                   "  #2: <helper> << %meh = add i32 %0, %0 >> [ meh = 14 ]\n" );
    }

    Test describe6()
    {
        divine::Blob b = _ith( code_callret(), 3 );
        assert_eq( _descr( code_callret(), b ),
                   "thread 0:\n  #1: <testf> << %meh = call i32 @helper() >> [ meh = 42 ]\n" );
    }

    Test memory1()
    {
        divine::Blob b = _ith( code_mem(), 2 );
        assert_eq( _descr( code_mem(), b ),
                   "thread 0:\n  #1: <testf> << br label %tail >> [ foo = @(0:0| 33) ]\n" );
    }

    Test idempotency()
    {
        Function *f = code_loop();
        Interpreter interpreter( pool, 0, bitcode() );
        divine::Blob b1 = interpreter.initial( f ), b2;
        interpreter.rewind( b1 );
        b2 = interpreter.state.snapshot();
        assert( pool.equal( b1, b2 ) );
    }
};
