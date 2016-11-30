// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/PassManager.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/Casting.h>
DIVINE_UNRELAX_WARNINGS

#include <divine/cc/clang.hpp>
#include <divine/rt/runtime.hpp>

#include <lart/support/pass.h>
#include <lart/support/meta.h>
#include <lart/support/util.h>
#include <lart/support/query.h>

#include <lart/abstract/passes.h>
#include <lart/abstract/types.h>

#include <brick-string>

#include <fstream>
#include <string>

namespace lart {
namespace abstract {

PassMeta abstraction_pass();
PassMeta substitution_pass();
PassMeta full_abstraction_pass();

} /* namespace abstract */

#ifdef BRICK_UNITTEST_REG
namespace t_abstract {

auto compile( std::string s ) {
    static std::shared_ptr< llvm::LLVMContext > ctx( new llvm::LLVMContext );
    divine::cc::Compiler c( ctx );
    c.mapVirtualFile( "main.c", s );
    return c.compileModule( "main.c" ).release();
}

std::string load( std::string path ) {
    std::ifstream file( path );
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

llvm::Module * test_abstraction( std::string s ) {
    auto m = compile( s );

    llvm::ModulePassManager manager;

    std::string opt = "abstract";
    abstract::abstraction_pass().create( manager, opt );

    manager.run( *m );
    return m;
}

llvm::Module * test_substitution( std::string s ) {
    std::string test_path;
    std::string abstraction;

    divine::rt::each( [&]( auto path, auto src ) {
        if ( brick::string::endsWith( path, "empty-abstraction.h" ) )
            abstraction = src;
    } );

    auto m = compile( abstraction + s );
    llvm::ModulePassManager manager;

    std::string opt = "test";
    abstract::abstraction_pass().create( manager, opt );
    abstract::substitution_pass().create( manager, opt );

    manager.run( *m );

    return m;
}

const std::string annotation =
                  "#define __test __attribute__((__annotate__(\"lart.abstract.test\")))\n";

bool containsUndefValue( llvm::Function &f ) {
    for ( auto & bb : f ) {
        for ( auto & i : bb ) {
            if ( llvm::isa< llvm::UndefValue >( i ) )
                return true;
            for ( auto & op : i.operands() )
                if ( llvm::isa< llvm::UndefValue >( op ) )
                    return true;
        }
    }

    return false;
}

bool containsUndefValue( llvm::Module &m ) {
    for ( auto & f : m ) {
        if ( containsUndefValue( f ) )
            return true;
    }

    return false;
}

struct Abstraction {
    TEST( simple ) {
        auto s = "int main() { return 0; }";
        test_abstraction( annotation + s );
    }

    TEST( create ) {
        auto s = R"(int main() {
                        __test int abs;
                        return 0;
                    })";
        auto m = test_abstraction( annotation + s );
        auto f = m->getFunction( "lart.abstract.alloca.i32" );
        ASSERT( f->hasOneUse() );
    }

    //phi node tests
    TEST( phi ) {
        auto s = R"(int main() {
                        __test int x = 0;
                        __test int y = 42;
                        int z = x || y;
                        return 0;
                    })";
        auto m = test_abstraction( annotation + s );
        auto nodes = query::query( *m ).flatten().flatten()
                           .map( query::refToPtr )
                           .map( query::llvmdyncast< llvm::PHINode > )
                           .filter( query::notnull )
                           .freeze();
        ASSERT_EQ( nodes.size(), 1 );
        ASSERT_EQ( nodes[0]->getNumIncomingValues(), 2 );
    }

    //call arg test
    TEST( call ) {
        auto s = R"(int call( int arg ) { return arg; }
                    int main() {
                        __test int abs;
                        int ret = call( abs );
                        return 0;
                    })";
        auto m = test_abstraction( annotation + s );

        auto main = m->getFunction( "main" );
        auto alloca = m->getFunction( "lart.abstract.alloca.i32" );
        auto abstract_call = m->getFunction( "call.2.3" );

        auto i32_t = llvm::Type::getInt32Ty( m->getContext() );

        ASSERT_EQ( abstract_call->getReturnType()
                 , alloca->getReturnType()->getPointerElementType() );
        ASSERT_EQ( abstract_call->getFunctionType()->getParamType( 0 )
                 , alloca->getReturnType()->getPointerElementType() );

        ASSERT_EQ( main->getReturnType(), i32_t );
        ASSERT( ! containsUndefValue( *m ) );
    }

    //return test + back propagation
    TEST( back_propagation ) {
        auto s = R"(int call() {
                        __test int x;
                        return x;
                    }
                    int main() {
                        int ret = call();
                        return 0;
                    })";
        auto m = test_abstraction( annotation + s );
        auto abstract_call = m->getFunction( "call.2" );
        auto alloca = m->getFunction( "lart.abstract.alloca.i32" );

        ASSERT_EQ( abstract_call->getReturnType()
                 , alloca->getReturnType()->getPointerElementType() );


    }

    //tristate test
    TEST( tristate ) {
        auto s = R"(int main() {
                        __test int x;
                        if ( x > 0 )
                            return 42;
                        else
                            return 0;
                    })";
        auto m = test_abstraction( annotation + s );

        auto sgt = m->getFunction( "lart.abstract.icmp.sgt.i32" );
        ASSERT_EQ( sgt->getReturnType(), m->getTypeByName( "lart.tristate" ) );

        auto lower = m->getFunction( "lart.tristate.lower" );
        ASSERT_EQ( lower->user_begin()->getOperand( 0 ), *sgt->user_begin() );
    }

    //lift test
    TEST( lift ) {
        auto s = R"(int main() {
                        __test int x;
                        return x + 42;
                    })";
        auto m = test_abstraction( annotation + s );
        auto lift = m->getFunction( "lart.abstract.lift.i32" )->user_begin();
        auto val = llvm::cast< llvm::ConstantInt >( lift->getOperand( 0 ) );
        ASSERT( val->equalsInt( 42 ) );
    }

    //replace lift test
    TEST( lift_replace ) {
        auto s = R"(int main() {
                        __test int x;
                        __test int y;
                        return x + y;
                    })";
        auto m = test_abstraction( annotation + s );
        auto lift = m->getFunction( "lart.abstract.lift.i32" );
        ASSERT( lift->hasOneUse() );
    }

    //switch - select test
    TEST( switch_test ) {
        auto s = R"(int main() {
                        __test int x;
                        int i = 0;
                        switch( x ) {
                            case 0: i = x; break;
                            case 1: i = (-x); break;
                            case 2: i = 42; break;
                        }
                    })";
        test_abstraction( annotation + s );
        /* TODO asserts */
    }
};

struct Substitution {
    TEST( simple ) {
        auto s = "int main() { return 0; }";
        test_substitution( annotation + s );
    }

    TEST( create ) {
        auto s = R"(int main() {
                        __test int abs;
                        return 0;
                    })";
        auto m = test_substitution( annotation + s );
        auto f = m->getFunction( "__abstract_test_alloca" );
        ASSERT( f->hasNUses( 2 )  );
    }
};

} /* namespace t_abstract */
#endif

} /* namespace lart */
