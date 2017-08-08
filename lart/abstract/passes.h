// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/PassManager.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/Casting.h>
DIVINE_UNRELAX_WARNINGS

#include <divine/cc/clang.hpp>
#include <divine/cc/compile.hpp>
#include <divine/cc/options.hpp>
#include <divine/rt/runtime.hpp>

#include <lart/support/pass.h>
#include <lart/support/meta.h>
#include <lart/support/util.h>
#include <lart/support/query.h>

#include <lart/abstract/passes.h>
#include <lart/abstract/types/common.h>
#include <lart/abstract/types/utils.h>
#include <lart/abstract/types/scalar.h>
#include <lart/abstract/intrinsic.h>

#include <brick-string>
#include <brick-llvm>

#include <fstream>
#include <string>

#include <lart/abstract/abstraction.h>
#include <lart/abstract/assume.h>
#include <lart/abstract/bcp.h>
#include <lart/abstract/substitution.h>

namespace lart {
namespace abstract {

    PassMeta abstraction_pass();
    PassMeta assume_pass();
    PassMeta bcp_pass();
    PassMeta substitution_pass();

    inline std::vector< PassMeta > passes() {
        return { abstraction_pass(), assume_pass(), bcp_pass(), substitution_pass() };
    }
}

#ifdef BRICK_UNITTEST_REG
namespace t_abstract {

using Compile = divine::cc::Compile;
using ModulePtr = Compile::ModulePtr;
void mapVirtualFile( Compile & c, const std::string & path, const std::string & src ) {
    c.setupFS( [&]( std::function< void( std::string, const std::string & ) > yield ) {
        yield( path, src );
    } );
}

namespace rt = divine::rt;

std::string source( std::string fileName ) {
    std::string res;
    rt::each( [&]( auto path, auto src ) {
        if ( brick::string::endsWith( path, fileName ) )
            res = std::string( src );
    } );
    return res;
}

void setupFS( Compile & c ) {
	auto each = [&]( auto filter ) {
        return [&filter]( std::function< void( std::string, rt::string_view ) > yield ) {
            rt::each( filter( yield ) );
        };
    };

    c.setupFS( each( [&]( std::function< void( std::string, rt::string_view ) > yield ) {
        return [&]( auto path, auto src ) {
			if ( !brick::string::endsWith( path, ".bc" ) )
				yield( path, src );
		};
	} ) );
}

ModulePtr compile( const std::string & src,
                   const std::vector< std::string > & link = {},
                   const std::vector< std::string > & headers = {} )
{
    static std::shared_ptr< llvm::LLVMContext > ctx( new llvm::LLVMContext );
    const bool dont_link = false;
    const bool verbose = false;
    Compile c( { dont_link, verbose }, ctx );

    mapVirtualFile( c, "main.cpp", src );
    for ( const auto & f : link )
        mapVirtualFile( c, f, source( f ) );
    for ( const auto & f : headers )
        mapVirtualFile( c, f, source( f ) );

    setupFS( c );
    std::vector< std::string > flags = { "-std=c++14" };

    brick::llvm::Linker linker;
    linker.link( c.compile( "main.cpp", flags ) );
    for ( const auto & f : link )
        linker.link( c.compile( f, flags ) );
    return linker.take();
}

std::string load( const std::string & path ) {
    std::ifstream file( path );
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

ModulePtr test_abstraction( const std::string & src ) {
    auto m = compile( src );

    llvm::ModulePassManager manager;

    abstract::abstraction_pass().create( manager, "" );

    manager.run( *m );
    return m;
}

ModulePtr test_assume( const std::string & src ) {
    auto m = compile( src );

    llvm::ModulePassManager manager;

    abstract::abstraction_pass().create( manager, "" );
    abstract::assume_pass().create( manager, "" );

    manager.run( *m );
    return m;
}

ModulePtr test_bcp( const std::string & src ) {
    auto m = compile( src );

    llvm::ModulePassManager manager;

    abstract::abstraction_pass().create( manager, "" );
    abstract::assume_pass().create( manager, "" );
    abstract::bcp_pass().create( manager, "" );

    manager.run( *m );
    return m;
}

ModulePtr test_substitution( const std::string & src,
                             std::vector< std::string > & link,
                             std::vector< std::string > & headers )
{
    auto m = compile( src, link, headers );
    llvm::ModulePassManager manager;

    abstract::abstraction_pass().create( manager, "" );
    abstract::assume_pass().create( manager, "" );
    abstract::bcp_pass().create( manager, "" );
    abstract::substitution_pass().create( manager, "" );

    manager.run( *m );

    return m;
}

ModulePtr test_symbolic_substitution( const std::string & src ) {
    std::vector< std::string > link = { "sym.cpp", "tristate.cpp", "formula.cpp" };
    std::vector< std::string > headers = { "sym.h", "tristate.h", "common.h", "formula.h" };
    return test_substitution( src, link, headers );
}

namespace {
const std::string annotation =
                  "#define __sym __attribute__((__annotate__(\"lart.abstract.sym\")))\n";

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

bool liftingPointer( llvm::Module &m ) {
    return query::query( m ).flatten().flatten()
           .map( query::refToPtr )
           .map( query::llvmdyncast< llvm::CallInst > )
           .filter( query::notnull )
           .filter( [&]( llvm::CallInst * call ) {
                return abstract::isLift( call );
            } )
            .any( []( llvm::CallInst * call ) {
                return call->getOperand( 0 )->getType()
                           ->isPointerTy();
            } );
}

} //empty namespace

using namespace abstract;

struct Abstraction {

    TEST( simple ) {
        auto s = "int main() { return 0; }";
        test_abstraction( annotation + s );
    }

    TEST( create ) {
        auto s = R"(int main() {
                        __sym int abs;
                        return 0;
                    })";
        auto m = test_abstraction( annotation + s );
        auto f = m->getFunction( "lart.sym.alloca.i32" );
        ASSERT( f->hasOneUse() );
    }

    TEST( types ) {
        auto s = R"(int main() {
                        __sym short abs_s;
                        __sym int abs;
                        __sym long abs_l;
                        return 0;
                    })";
        auto m = test_abstraction( annotation + s );
        auto f16 = m->getFunction( "lart.sym.alloca.i16" );
        ASSERT( f16->hasOneUse() );
        auto f32 = m->getFunction( "lart.sym.alloca.i32" );
        ASSERT( f32->hasOneUse() );
        auto f64 = m->getFunction( "lart.sym.alloca.i64" );
        ASSERT( f64->hasOneUse() );
    }

    TEST( binary_ops ) {
        auto s = R"(int main() {
                        __sym int abs;
                        int a = abs + 42;
                        int b = abs + a;
                        return 0;
                    })";
        auto m = test_abstraction( annotation + s );
        ASSERT( ! liftingPointer( *m ) );
    }

    TEST( phi ) {
        auto s = R"(int main() {
                        __sym int x = 0;
                        __sym int y = 42;
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
        ASSERT( ! liftingPointer( *m ) );
    }

    TEST( call_simple ) {
        auto s = R"(int call( int arg ) { return arg; }
                    int main() {
                        __sym int abs;
                        int ret = call( abs );
                        return 0;
                    })";
        auto m = test_abstraction( annotation + s );
        auto main = m->getFunction( "main" );
        auto alloca = m->getFunction( "lart.sym.alloca.i32" );
        auto call = m->getFunction( "_Z4calli.2" );
        ASSERT_EQ( call->getNumUses(), 1 );
        ASSERT_EQ( call->getReturnType(), alloca->getReturnType()->getPointerElementType() );
        ASSERT_EQ( call->getFunctionType()->getParamType( 0 )
                 , alloca->getReturnType()->getPointerElementType() );

        ASSERT_EQ( main->getReturnType(), llvm::Type::getInt32Ty( m->getContext() ) );
        ASSERT( ! containsUndefValue( *m ) );
        ASSERT( ! liftingPointer( *m ) );
    }

    TEST( call_twice ) {
        auto s = R"(int call( int arg ) { return arg; }
                    int main() {
                        __sym int abs;
                        int ret = call( abs );
                        ret = call( ret );
                        return 0;
                    })";
        auto m = test_abstraction( annotation + s );
        auto main = m->getFunction( "main" );
        auto alloca = m->getFunction( "lart.sym.alloca.i32" );
        auto call = m->getFunction( "_Z4calli.2" );
        ASSERT_EQ( call->getNumUses(), 2 );
        ASSERT_EQ( call->getReturnType(), alloca->getReturnType()->getPointerElementType() );
        ASSERT_EQ( call->getFunctionType()->getParamType( 0 )
                 , alloca->getReturnType()->getPointerElementType() );

        ASSERT_EQ( main->getReturnType(), llvm::Type::getInt32Ty( m->getContext() ) );
        ASSERT( ! containsUndefValue( *m ) );
        ASSERT( ! liftingPointer( *m ) );
    }

    TEST( call_independent ) {
        auto s = R"(int call( int arg ) { return arg; }
                    int main() {
                        __sym int abs;
                        int ret = call( abs );
                        ret = call( 10 );
                        return 0;
                    })";
        auto m = test_abstraction( annotation + s );
        auto main = m->getFunction( "main" );
        auto alloca = m->getFunction( "lart.sym.alloca.i32" );
        auto call = m->getFunction( "_Z4calli.2" );
        auto call_normal = m->getFunction( "_Z4calli" );
        ASSERT_EQ( call->getNumUses(), 1 );
        ASSERT_EQ( call_normal->getNumUses(), 1 );
        ASSERT_EQ( call->getReturnType(), alloca->getReturnType()->getPointerElementType() );
        ASSERT_EQ( call->getFunctionType()->getParamType( 0 )
                 , alloca->getReturnType()->getPointerElementType() );

        ASSERT_EQ( main->getReturnType(), llvm::Type::getInt32Ty( m->getContext() ) );
        ASSERT( ! containsUndefValue( *m ) );
        ASSERT( ! liftingPointer( *m ) );
    }

    TEST( call_two_args_mixed ) {
        auto s = R"(int call( int a, int b ) { return a + b; }
                    int main() {
                        __sym int abs;
                        int normal = 10;
                        int ret = call( abs, normal );
                        return 0;
                    })";
        auto m = test_abstraction( annotation + s );
        auto main = m->getFunction( "main" );
        auto alloca = m->getFunction( "lart.sym.alloca.i32" );
        auto call = m->getFunction( "_Z4callii.2" );
        ASSERT_EQ( call->getNumUses(), 1 );
        ASSERT_EQ( call->getReturnType(), alloca->getReturnType()->getPointerElementType() );
        ASSERT_EQ( call->getFunctionType()->getParamType( 0 )
                 , alloca->getReturnType()->getPointerElementType() );
        ASSERT_EQ( call->getFunctionType()->getParamType( 1 )
                 , llvm::Type::getInt32Ty( m->getContext() ) );
        ASSERT_EQ( main->getReturnType(), llvm::Type::getInt32Ty( m->getContext() ) );
        ASSERT( ! containsUndefValue( *m ) );
        ASSERT( ! liftingPointer( *m ) );
    }

    TEST( call_two_args_abstract ) {
        auto s = R"(int call( int a, int b ) { return a + b; }
                    int main() {
                        __sym int a;
                        __sym int b;
                        int ret = call( a, b );
                        return 0;
                    })";
        auto m = test_abstraction( annotation + s );
        auto main = m->getFunction( "main" );
        auto alloca = m->getFunction( "lart.sym.alloca.i32" );
        auto call = m->getFunction( "_Z4callii.2" );
        ASSERT_EQ( call->getNumUses(), 1 );
        ASSERT_EQ( call->getReturnType(), alloca->getReturnType()->getPointerElementType() );
        ASSERT_EQ( call->getFunctionType()->getParamType( 0 )
                 , alloca->getReturnType()->getPointerElementType() );
        ASSERT_EQ( call->getFunctionType()->getParamType( 1 )
                 , alloca->getReturnType()->getPointerElementType() );

        ASSERT_EQ( main->getReturnType(), llvm::Type::getInt32Ty( m->getContext() ) );
        ASSERT( ! containsUndefValue( *m ) );
        ASSERT( ! liftingPointer( *m ) );
    }

    TEST( call_preserve_original_function ) {
        auto s = R"(int call( int a ) { return a; }
                    int main() {
                        __sym int a;
                        call( a );
                        call( 0 );
                        return 0;
                    })";
        auto m = test_abstraction( annotation + s );
        auto main = m->getFunction( "main" );
        auto alloca = m->getFunction( "lart.sym.alloca.i32" );
        auto call = m->getFunction( "_Z4calli.2" );
        ASSERT_EQ( call->getNumUses(), 1 );
        ASSERT_EQ( call->getReturnType(), alloca->getReturnType()->getPointerElementType() );
        ASSERT_EQ( call->getFunctionType()->getParamType( 0 )
                 , alloca->getReturnType()->getPointerElementType() );

        auto call2 = m->getFunction( "_Z4calli" );
        ASSERT_EQ( call2->getNumUses(), 1 );
        ASSERT_EQ( call2->getReturnType(), llvm::Type::getInt32Ty( m->getContext() ) );
        ASSERT_EQ( call2->getFunctionType()->getParamType( 0 )
                 , llvm::Type::getInt32Ty( m->getContext() ) );

        bool contains_abstract_calls = query::query( *call2 ).flatten()
            .map( query::llvmdyncast< llvm::CallInst > )
            .filter( query::notnull )
            .any( []( llvm::CallInst * call ) {
                return call->getCalledFunction()->getName().startswith( "lart.sym" );
            } );
        ASSERT( !contains_abstract_calls );

        ASSERT_EQ( main->getReturnType(), llvm::Type::getInt32Ty( m->getContext() ) );

        ASSERT( ! containsUndefValue( *m ) );
        ASSERT( ! liftingPointer( *m ) );
    }

    TEST( call_two_args_multiple_times ) {
        auto s = R"(int call( int a, int b ) { return a + b; }
                    int main() {
                        __sym int a;
                        __sym int b;
                        int ret, c = 0, d = 1;
                        ret = call( a, b );
                        ret = call( a, c );
                        ret = call( c, d );
                        return 0;
                    })";
        auto m = test_abstraction( annotation + s );
        auto main = m->getFunction( "main" );
        auto alloca = m->getFunction( "lart.sym.alloca.i32" );
        auto call = m->getFunction( "_Z4callii.3" );
        ASSERT_EQ( call->getNumUses(), 1 );
        ASSERT_EQ( call->getReturnType(), alloca->getReturnType()->getPointerElementType() );
        ASSERT_EQ( call->getFunctionType()->getParamType( 0 )
                 , alloca->getReturnType()->getPointerElementType() );
        ASSERT_EQ( call->getFunctionType()->getParamType( 1 )
                 , alloca->getReturnType()->getPointerElementType() );
        auto call2 = m->getFunction( "_Z4callii.2" );
        ASSERT_EQ( call2->getNumUses(), 1 );
        ASSERT_EQ( call2->getReturnType(), alloca->getReturnType()->getPointerElementType() );
        ASSERT_EQ( call2->getFunctionType()->getParamType( 0 )
                 , alloca->getReturnType()->getPointerElementType() );
        ASSERT_EQ( call2->getFunctionType()->getParamType( 1 )
                 , llvm::Type::getInt32Ty( m->getContext() ) );
        auto call3 = m->getFunction( "_Z4callii" );
        ASSERT_EQ( call3->getNumUses(), 1 );
        ASSERT_EQ( call3->getReturnType(), llvm::Type::getInt32Ty( m->getContext() ) );
        ASSERT_EQ( call3->getFunctionType()->getParamType( 0 )
                 , llvm::Type::getInt32Ty( m->getContext() ) );
        ASSERT_EQ( call3->getFunctionType()->getParamType( 1 )
                 , llvm::Type::getInt32Ty( m->getContext() ) );

        ASSERT_EQ( main->getReturnType(), llvm::Type::getInt32Ty( m->getContext() ) );
        ASSERT( ! containsUndefValue( *m ) );
        ASSERT( ! liftingPointer( *m ) );
    }

    TEST( call_two_times_from_different_source ) {
        auto s = R"(int call1( int x ) { return x; }
                    int call2( int x ) { return call1( x ); }
                    int main() {
                        __sym int x;
                        call2( x );
                        call1( x );
                        return 0;
                    })";
        auto m = test_abstraction( annotation + s );
        auto alloca = m->getFunction( "lart.sym.alloca.i32" );
        auto call1 = m->getFunction( "_Z5call1i.2" );
        ASSERT_EQ( call1->getNumUses(), 2 );
        ASSERT_EQ( call1->getReturnType(), alloca->getReturnType()->getPointerElementType() );
        ASSERT_EQ( call1->getFunctionType()->getParamType( 0 )
                 , alloca->getReturnType()->getPointerElementType() );
        auto call2 = m->getFunction( "_Z5call2i.3" );
        ASSERT_EQ( call2->getNumUses(), 1 );
        ASSERT_EQ( call2->getReturnType(), alloca->getReturnType()->getPointerElementType() );
        ASSERT_EQ( call2->getFunctionType()->getParamType( 0 )
                 , alloca->getReturnType()->getPointerElementType() );
        ASSERT( ! containsUndefValue( *m ) );
        ASSERT( ! liftingPointer( *m ) );
    }

    TEST( propagate_from_call_not_returning_abstract ) {
        auto s = R"(int call( int x ) { return 10; }
                    int main() {
                        __sym int x;
                        int ret = call( x );
                        ret = call( ret );
                        return 0;
                    })";
        auto m = test_abstraction( annotation + s );
        auto alloca = m->getFunction( "lart.sym.alloca.i32" );
        auto calla = m->getFunction( "_Z4calli.2" );
        ASSERT_EQ( calla->getNumUses(), 1);
        ASSERT_EQ( calla->getReturnType()
                 , llvm::Type::getInt32Ty( m->getContext() ) );
        ASSERT_EQ( calla->getFunctionType()->getParamType( 0 )
                 , alloca->getReturnType()->getPointerElementType() );
        auto call = m->getFunction( "_Z4calli" );
        ASSERT_EQ( call->getNumUses(), 1 );
        ASSERT_EQ( call->getReturnType()
                 , llvm::Type::getInt32Ty( m->getContext() ) );
        ASSERT_EQ( call->getFunctionType()->getParamType( 0 )
                 , llvm::Type::getInt32Ty( m->getContext() ) );
        ASSERT( ! containsUndefValue( *m ) );
        ASSERT( ! liftingPointer( *m ) );
    }

    TEST( call_propagate_1 ) {
        auto s = R"(int call() {
                        __sym int x;
                        return x;
                    }
                    int main() {
                        int ret = call();
                        return 0;
                    })";
        auto m = test_abstraction( annotation + s );
        auto abstract_call = m->getFunction( "_Z4callv.2" );
        auto alloca = m->getFunction( "lart.sym.alloca.i32" );

        ASSERT_EQ( abstract_call->getReturnType()
                 , alloca->getReturnType()->getPointerElementType() );
        ASSERT( ! containsUndefValue( *m ) );
        ASSERT( ! liftingPointer( *m ) );
    }

    TEST( call_propagate_back_multiple_times ) {
        auto s = R"(int nondet() {
                        __sym int x;
                        return x;
                    }
                    int add() {
                        int x = nondet();
                        int y = nondet();
                        return x + y;
                    }
                    int main() {
                        int x = add();
                        int y = add();
                        return 0;
                    })";
        auto m = test_abstraction( annotation + s );
        auto main = m->getFunction( "main" );
        auto alloca = m->getFunction( "lart.sym.alloca.i32" );
        auto add = m->getFunction( "_Z3addv.3" );
        ASSERT_EQ( add->getNumUses(), 2 );
        for ( const auto & u : add->users() ) {
            ASSERT_EQ( llvm::cast< llvm::CallInst >( u )->getParent()->getParent()
                     , main );
        }
        ASSERT_EQ( add->getReturnType(), alloca->getReturnType()->getPointerElementType() );
        auto nondet = m->getFunction( "_Z6nondetv.2" );
        ASSERT_EQ( nondet->getNumUses(), 2 );
        for ( const auto & u : nondet->users() ) {
            ASSERT_EQ( llvm::cast< llvm::CallInst >( u )->getParent()->getParent()
                     , add );
        }
        ASSERT_EQ( nondet->getReturnType(), alloca->getReturnType()->getPointerElementType() );
        ASSERT( ! containsUndefValue( *m ) );
        ASSERT( ! liftingPointer( *m ) );
    }

    TEST( call_propagate_deeper_1 ) {
        auto s = R"(int call2( int x ) {
                        return x * x;
                    }
                    int call1() {
                        __sym int x;
                        return x;
                    }
                    int main() {
                        int ret = call1();
                        call2( ret );
                        return 0;
                    })";
        auto m = test_abstraction( annotation + s );
        auto alloca = m->getFunction( "lart.sym.alloca.i32" );
        auto call1 = m->getFunction( "_Z5call1v.3" );
        auto call2 = m->getFunction( "_Z5call2i.2" );
        ASSERT_EQ( call1->getReturnType(), alloca->getReturnType()->getPointerElementType() );
        ASSERT_EQ( call2->getReturnType(), alloca->getReturnType()->getPointerElementType() );
        ASSERT_EQ( call2->getFunctionType()->getParamType( 0 )
                 , alloca->getReturnType()->getPointerElementType() );
        ASSERT( ! containsUndefValue( *m ) );
        ASSERT( ! liftingPointer( *m ) );
    }

    TEST( call_propagate_deeper_2 ) {
        auto s = R"(
                    int call4( int x ) { return x; }
                    int call3( int x ) { return x; }
                    int call2( int x ) {
                        return call3( x ) * call4( x );
                    }
                    int call1() {
                        __sym int x;
                        return x;
                    }
                    int main() {
                        int ret = call1();
                        call2( ret );
                        return 0;
                    })";
        auto m = test_abstraction( annotation + s );
        auto alloca = m->getFunction( "lart.sym.alloca.i32" );
        auto call1 = m->getFunction( "_Z5call1v.5" );
        auto call2 = m->getFunction( "_Z5call2i.4" );
        auto call3 = m->getFunction( "_Z5call3i.3" );
        auto call4 = m->getFunction( "_Z5call4i.2" );
        ASSERT_EQ( call1->getReturnType(), alloca->getReturnType()->getPointerElementType() );
        ASSERT_EQ( call2->getReturnType(), alloca->getReturnType()->getPointerElementType() );
        ASSERT_EQ( call2->getFunctionType()->getParamType( 0 )
                 , alloca->getReturnType()->getPointerElementType() );
        ASSERT_EQ( call3->getReturnType(), alloca->getReturnType()->getPointerElementType() );
        ASSERT_EQ( call3->getFunctionType()->getParamType( 0 )
                 , alloca->getReturnType()->getPointerElementType() );
        ASSERT_EQ( call4->getReturnType(), alloca->getReturnType()->getPointerElementType() );
        ASSERT_EQ( call4->getFunctionType()->getParamType( 0 )
                 , alloca->getReturnType()->getPointerElementType() );
        ASSERT( ! containsUndefValue( *m ) );
        ASSERT( ! liftingPointer( *m ) );
    }

    TEST( tristate ) {
        auto s = R"(int main() {
                        __sym int x;
                        if ( x > 0 )
                            return 42;
                        else
                            return 0;
                    })";
        auto m = test_abstraction( annotation + s );
        auto sgt = m->getFunction( "lart.sym.icmp_sgt.i32" );
        auto expected = liftTypeLLVM( BoolType( m->getContext() ), SymbolicDomain );
        ASSERT_EQ( expected, sgt->getReturnType() );
        auto to_tristate = m->getFunction( "lart.sym.bool_to_tristate" );
        ASSERT_EQ( to_tristate->user_begin()->getOperand( 0 ), *sgt->user_begin() );
        auto lower = m->getFunction( "lart.tristate.lower" );
        ASSERT_EQ( lower->user_begin()->getOperand( 0 ), *to_tristate->user_begin() );
        ASSERT( ! containsUndefValue( *m ) );
        ASSERT( ! liftingPointer( *m ) );
    }

    TEST( lift ) {
        auto s = R"(int main() {
                        __sym int x;
                        return x + 42;
                    })";
        auto m = test_abstraction( annotation + s );
        auto lift = m->getFunction( "lart.sym.lift.i32" )->user_begin();
        auto val = llvm::cast< llvm::ConstantInt >( lift->getOperand( 0 ) );
        ASSERT( val->equalsInt( 42 ) );
        ASSERT( ! containsUndefValue( *m ) );
        ASSERT( ! liftingPointer( *m ) );
    }

    TEST( lift_replace ) {
        auto s = R"(int main() {
                        __sym int x;
                        __sym int y;
                        return x + y;
                    })";
        auto m = test_abstraction( annotation + s );
        ASSERT( ! containsUndefValue( *m ) );
        ASSERT( ! liftingPointer( *m ) );
    }

    TEST( switch_test ) {
        auto s = R"(int main() {
                        __sym int x;
                        int i = 0;
                        switch( x ) {
                            case 0: i = x; break;
                            case 1: i = (-x); break;
                            case 2: i = 42; break;
                        }
                    })";
        auto m = test_abstraction( annotation + s );
        ASSERT( ! containsUndefValue( *m ) );
        ASSERT( ! liftingPointer( *m ) );
    }

    TEST( loop_test ) {
        auto s = R"(int main() {
                        __sym int x;
                        for ( int i = 0; i < x; ++i )
                            for ( int j = 0; j < x; ++j )
                                for ( int k = 0; k < x; ++k ) {
                                    __sym int y = i * j *k;
                                }
                    })";
        auto m = test_abstraction( annotation + s );
        ASSERT( ! containsUndefValue( *m ) );
        ASSERT( ! liftingPointer( *m ) );
    }

    TEST( loop_test_2 ) {
        auto s = R"(int main() {
                      unsigned N = 1000;
                      unsigned long long Q = 10000000000ULL;
                      unsigned long long rem = 0;
                      __sym unsigned i;
                      for (i = 1; i < N; i++) {
                        unsigned long long r = 1;
                        unsigned j;
                        for (j = 0; j < i; j++) {
                          r = (r * i) % Q;
                        }
                        rem = (rem + r) % Q;
                      }

                      return 0;
                    })";
        auto m = test_abstraction( annotation + s );
        ASSERT( ! containsUndefValue( *m ) );
        ASSERT( ! liftingPointer( *m ) );
    }

    TEST( recursion_direct ) {
        auto s = R"(
                    int call( int x ) {
                        if ( x < 100 )
                            return x;
                        else
                            return call( x - 1 );
                    }
                    int main() {
                        __sym unsigned x;
                        call( x );
                        return 0;
                    })";
        auto m = test_abstraction( annotation + s );
        auto main = m->getFunction( "main" );
        auto alloca = m->getFunction( "lart.sym.alloca.i32" );
        auto call = m->getFunction( "_Z4calli.2" );
        ASSERT_EQ( call->getReturnType(), alloca->getReturnType()->getPointerElementType() );
        ASSERT_EQ( call->getFunctionType()->getParamType( 0 )
                 , alloca->getReturnType()->getPointerElementType() );
        auto call_in_main = query::query( *main ).flatten()
            .map( query::llvmdyncast< llvm::CallInst > )
            .filter( query::notnull )
            .filter( []( llvm::CallInst * i ) { return !i->getCalledFunction()->isIntrinsic(); } )
            .filter( [&]( llvm::CallInst * i ) { return !abstract::isIntrinsic( i ); } )
            .all( [&] ( llvm::CallInst * i ) { return i->getCalledFunction() == call; } );
        ASSERT( call_in_main );
        auto call_in_call = query::query( *call ).flatten()
            .map( query::llvmdyncast< llvm::CallInst > )
            .filter( query::notnull )
            .filter( []( llvm::CallInst * i ) { return !i->getCalledFunction()->isIntrinsic(); } )
            .filter( [&]( llvm::CallInst * i ) { return !abstract::isIntrinsic( i ); } )
            .all( [&] ( llvm::CallInst * i ) { return i->getCalledFunction() == call; } );
        ASSERT( call_in_call );
        ASSERT_EQ( call->getNumUses(), 2 );
        ASSERT( ! containsUndefValue( *m ) );
        ASSERT( ! liftingPointer( *m ) );
    }

    TEST( recursion_multiple_times ) {
        auto s = R"(
                    void call( int x, int y ) {
                        if ( x != y ) {
                            int z = ( x + y ) / 2;
                            call( x, z - 1 );
                            call( z + 1, y );
                        }
                    }
                    int main() {
                        __sym unsigned x;
                        call( x, 100 );
                        return 0;
                    })";
        auto m = test_abstraction( annotation + s );
        auto main = m->getFunction( "main" );
        auto alloca = m->getFunction( "lart.sym.alloca.i32" );
        auto call = m->getFunction( "_Z4callii.3" );
        ASSERT_EQ( call->getNumUses(), 2 );
        ASSERT_EQ( call->getFunctionType()->getParamType( 0 )
                 , alloca->getReturnType()->getPointerElementType() );
        ASSERT_EQ( call->getFunctionType()->getParamType( 1 )
                 , llvm::Type::getInt32Ty( m->getContext() ) );
        auto call2 = m->getFunction( "_Z4callii.2" );
        ASSERT_EQ( call2->getFunctionType()->getParamType( 0 )
                 , alloca->getReturnType()->getPointerElementType() );
        ASSERT_EQ( call2->getFunctionType()->getParamType( 1 )
                 , alloca->getReturnType()->getPointerElementType() );
        auto test_function = [&] ( llvm::Function * fn ) {
            return query::query( *fn ).flatten()
            .map( query::llvmdyncast< llvm::CallInst > )
            .filter( query::notnull )
            .filter( []( llvm::CallInst * i ) { return !i->getCalledFunction()->isIntrinsic(); } )
            .filter( [&]( llvm::CallInst * i ) { return !abstract::isIntrinsic( i ); } )
            .all( [&] ( llvm::CallInst * i ) { return i->getCalledFunction() == call ||
                                                      i->getCalledFunction() == call2; } );
        };
        ASSERT( test_function( main ) );
        ASSERT( test_function( call ) );
        ASSERT( test_function( call2 ) );
        ASSERT( ! containsUndefValue( *m ) );
        ASSERT( ! liftingPointer( *m ) );
    }

    TEST( recursion_without_abstract_return ) {
        auto s = R"(
                    int call( int x, int y ) {
                        if ( y == 0 || x == 0 )
                            return x;
                        else return call(x - 1, y - 1);
                    }
                    int main() {
                        __sym unsigned x;
                        call( 100, x );
                        return 0;
                    })";
        auto m = test_abstraction( annotation + s );
        auto main = m->getFunction( "main" );
        auto alloca = m->getFunction( "lart.sym.alloca.i32" );
        auto call = m->getFunction( "_Z4callii.2" );
        ASSERT_EQ( call->getNumUses(), 2 );
        ASSERT_EQ( call->getReturnType()
                 , llvm::Type::getInt32Ty( m->getContext() ) );
        ASSERT_EQ( call->getFunctionType()->getParamType( 0 )
                 , llvm::Type::getInt32Ty( m->getContext() ) );
        ASSERT_EQ( call->getFunctionType()->getParamType( 1 )
                 , alloca->getReturnType()->getPointerElementType() );
        auto test_function = [&] ( llvm::Function * fn ) {
            return query::query( *fn ).flatten()
            .map( query::llvmdyncast< llvm::CallInst > )
            .filter( query::notnull )
            .filter( []( llvm::CallInst * i ) { return !i->getCalledFunction()->isIntrinsic(); } )
            .filter( [&]( llvm::CallInst * i ) { return !abstract::isIntrinsic( i ); } )
            .all( [&] ( llvm::CallInst * i ) { return i->getCalledFunction() == call; } );
        };
        ASSERT( test_function( main ) );
        ASSERT( test_function( call ) );
        ASSERT( ! containsUndefValue( *m ) );
        ASSERT( ! liftingPointer( *m ) );
    }

    TEST( struct_simple ) {
        auto s = R"(struct S { int x; };

                    int main() {
                        __sym int x;
                        S s;
                        s.x = x;
                    })";
        auto m = test_abstraction( annotation + s );
        auto main = m->getFunction( "main" );
        auto gep = query::query( *main ).flatten()
            .map( query::refToPtr )
            .map( query::llvmdyncast< llvm::GetElementPtrInst > )
            .filter( query::notnull )
            .freeze();
        auto alloca = m->getFunction( "lart.sym.alloca.i32" );
        ASSERT_EQ( alloca->getReturnType()->getPointerElementType(),
                   gep[0]->getResultElementType() );
        ASSERT( ! containsUndefValue( *m ) );
        ASSERT( ! liftingPointer( *m ) );
    }

    TEST( struct_complex ) {
        auto s = R"(struct S { int x, y, z; };

                    int main() {
                        __sym int x;
                        S s;
                        s.y = x;
                    })";
        auto m = test_abstraction( annotation + s );
        auto main = m->getFunction( "main" );
        auto gep = query::query( *main ).flatten()
            .map( query::refToPtr )
            .map( query::llvmdyncast< llvm::GetElementPtrInst > )
            .filter( query::notnull )
            .freeze();

        auto alloca = m->getFunction( "lart.sym.alloca.i32" );
        ASSERT_EQ( alloca->getReturnType()->getPointerElementType(),
                   gep[0]->getResultElementType() );
        ASSERT( ! containsUndefValue( *m ) );
        ASSERT( ! liftingPointer( *m ) );
    }

    TEST( struct_multiple_abstract_values ) {
        auto s = R"(struct S { int x, y, z; };

                    int main() {
                        __sym int x;
                        __sym int y;
                        S s;
                        s.x = x;
                        s.y = y;
                        s.z = 42;
                    })";
        auto m = test_abstraction( annotation + s );
        auto main = m->getFunction( "main" );
        auto gep = query::query( *main ).flatten()
            .map( query::refToPtr )
            .map( query::llvmdyncast< llvm::GetElementPtrInst > )
            .filter( query::notnull )
            .freeze();

        auto alloca = m->getFunction( "lart.sym.alloca.i32" );
        ASSERT_EQ( alloca->getReturnType()->getPointerElementType(),
                   gep[0]->getResultElementType() );
        ASSERT_EQ( alloca->getReturnType()->getPointerElementType(),
                   gep[1]->getResultElementType() );
        ASSERT_EQ( llvm::Type::getInt32Ty( m->getContext() ),
                   gep[2]->getResultElementType() );
        ASSERT( ! containsUndefValue( *m ) );
        ASSERT( ! liftingPointer( *m ) );
    }

    TEST( struct_with_pointer ) {
        auto s = R"(struct S {
                        int x;
                        int * y;
                    };

                    int main() {
                        __sym int x;
                        int y = 0;
                        S s;
                        s.x = x;
                        s.y = &y;
                    })";
        auto m = test_abstraction( annotation + s );
        auto main = m->getFunction( "main" );
        auto gep = query::query( *main ).flatten()
            .map( query::refToPtr )
            .map( query::llvmdyncast< llvm::GetElementPtrInst > )
            .filter( query::notnull )
            .freeze();

        auto alloca = m->getFunction( "lart.sym.alloca.i32" );
        ASSERT_EQ( alloca->getReturnType()->getPointerElementType(),
                   gep[0]->getResultElementType() );
        ASSERT( ! containsUndefValue( *m ) );
        ASSERT( ! liftingPointer( *m ) );
    }
};

struct Assume {
    bool isTristateAssume( llvm::Instruction * inst ) {
        if ( auto call = llvm::dyn_cast< llvm::CallInst >( inst ) ) {
            auto fn = call->getCalledFunction();
            return fn->hasName() && fn->getName().startswith( "lart.tristate.assume" );
        }
        return false;
    }

    bool isTrueAssume( llvm::Instruction * inst ) {
        return isTristateAssume( inst )
            && ( llvm::cast< llvm::Constant >( inst->getOperand( 1 ) )->isOneValue() );
    }

    bool isFalseAssume( llvm::Instruction * inst ) {
        return isTristateAssume( inst )
            && ( llvm::cast< llvm::Constant >( inst->getOperand( 1 ) )->isZeroValue() );
    }

    void testBranching( llvm::Instruction * lower ) {
        auto br = llvm::cast< llvm::BranchInst >( *lower->user_begin() );

        auto trueBB = br->getSuccessor( 0 );
        ASSERT( isTrueAssume( trueBB->begin() ) );
        auto falseBB = br->getSuccessor( 1 );
        ASSERT( isFalseAssume( falseBB->begin() ) );
    }

    TEST( simple ) {
        auto s = "int main() { return 0; }";
        test_assume( annotation + s );
    }

    TEST( tristate ) {
        auto s = R"(int main() {
                        __sym int x;
                        if ( x > 0 )
                            return 42;
                        else
                            return 0;
                    })";
        auto m = test_assume( annotation + s );
        auto icmp = m->getFunction( "lart.sym.icmp_sgt.i32" );
        auto expected = liftTypeLLVM( BoolType( m->getContext() ), SymbolicDomain );
        ASSERT_EQ( expected, icmp->getReturnType() );
        auto to_tristate = m->getFunction( "lart.sym.bool_to_tristate" );
        ASSERT_EQ( to_tristate->user_begin()->getOperand( 0 ), *icmp->user_begin() );
        auto lower = llvm::cast< llvm::Instruction >(
                     *m->getFunction( "lart.tristate.lower" )->user_begin() );
        ASSERT_EQ( lower->getOperand( 0 ), *to_tristate->user_begin() );

        testBranching( lower );
    }

    TEST( loop ) {
        auto s = R"(int main() {
                        __sym int abs;
                        while( abs ) {
                            ++abs;
                        }
                        return 0;
                    })";
        auto m = test_assume( annotation + s );

        auto icmp = m->getFunction( "lart.sym.icmp_ne.i32" );
        auto expected = liftTypeLLVM( BoolType( m->getContext() ), SymbolicDomain );
        ASSERT_EQ( expected, icmp->getReturnType() );
        auto to_tristate = m->getFunction( "lart.sym.bool_to_tristate" );
        ASSERT_EQ( to_tristate->user_begin()->getOperand( 0 ), *icmp->user_begin() );
        auto lower = llvm::cast< llvm::Instruction >(
                     *m->getFunction( "lart.tristate.lower" )->user_begin() );
        ASSERT_EQ( lower->getOperand( 0 ), *to_tristate->user_begin() );

        testBranching( lower );
    }
};

struct BCP {
    TEST( tristate ) {
        auto s = R"(int call() {
                        __sym int x;
                        __sym int y;
                        if ( x == 0 ) {
                            y = x;
                        }
                        while ( y < x )
                            y++;
                        return y;
                    }
                    int main() {
                        call();
                        return 0;
                    })";
        test_bcp( annotation + s );
    }
};

struct Substitution {
    TEST( simple ) {
        auto s = "int main() { return 0; }";
        test_symbolic_substitution( annotation + s );
    }

    TEST( create ) {
        auto s = R"(int main() {
                        __sym int abs;
                        return 0;
                    })";
        auto m = test_symbolic_substitution( annotation + s );
        auto f = m->getFunction( "__abstract_sym_alloca" );
        ASSERT( f->hasNUses( 2 ) );
    }

    TEST( types ) {
        auto s = R"(int main() {
                        __sym short abs_s;
                        __sym int abs;
                        __sym long abs_l;
                        return 0;
                    })";
        auto m = test_symbolic_substitution( annotation + s );
        auto alloca = m->getFunction( "__abstract_sym_alloca" );
        ASSERT( alloca->hasNUses( 4 ) );
    }

    TEST( binary_ops ) {
        auto s = R"(int main() {
                        __sym int abs;
                        int a = abs + 42;
                        int b = abs + a;
                        return 0;
                    })";
        auto m = test_symbolic_substitution( annotation + s );
        auto add = m->getFunction( "__abstract_sym_add" );
        ASSERT( add->hasNUses( 3 ) );

        auto lift = m->getFunction( "__abstract_sym_lift" )->user_begin();
        ASSERT( llvm::isa< llvm::ConstantInt >( lift->getOperand( 0 ) ) );
        ASSERT_EQ( add->user_begin()->getOperand( 1 ), *lift );
    }

    TEST( tristate ) {
        auto s = R"(int main() {
                        __sym int x;
                        if ( x > 0 )
                            return 42;
                        else
                            return 0;
                    })";
        auto m = test_symbolic_substitution( annotation + s );
        auto icmp = m->getFunction( "__abstract_sym_icmp_sgt" );
        ASSERT_EQ( icmp->getReturnType(),
                   m->getTypeByName( "union.sym::Formula" )->getPointerTo() );
        auto to_tristate = m->getFunction( "__abstract_sym_bool_to_tristate" );
        ASSERT_EQ( to_tristate->user_begin()->getOperand( 0 ), *icmp->user_begin() );
        auto lower = llvm::cast< llvm::Instruction >(
                     *m->getFunction( "__abstract_tristate_lower" )->user_begin() );
        ASSERT_EQ( lower->getOperand( 0 ), *to_tristate->user_begin() );
    }

    TEST( phi ) {
        auto s = R"(int main() {
                        __sym int x = 0;
                        __sym int y = 42;
                        int z = x || y;
                        return 0;
                    })";
        auto m = test_symbolic_substitution( annotation + s );
        auto main = m->getFunction( "main" );
        auto nodes = query::query( *main ).flatten()
                           .map( query::refToPtr )
                           .map( query::llvmdyncast< llvm::PHINode > )
                           .filter( query::notnull )
                           .freeze();

        ASSERT_EQ( nodes.size(), 1 );
        ASSERT_EQ( nodes[0]->getNumIncomingValues(), 2 );
    }

    TEST( switch_test ) {
        auto s = R"(int main() {
                        __sym int x;
                        int i = 0;
                        switch( x ) {
                            case 0: i = x; break;
                            case 1: i = (-x); break;
                            case 2: i = 42; break;
                        }
                    })";
        auto m = test_symbolic_substitution( annotation + s );
        //TODO asserts
    }

    TEST( lift_1 ) {
        auto s = R"(int main() {
                        __sym int x;
                        return x + 42;
                    })";
        auto m = test_symbolic_substitution( annotation + s );
        auto lift = m->getFunction( "__abstract_sym_lift" )->user_begin();
        auto val = llvm::cast< llvm::ConstantInt >( lift->getOperand( 0 ) );
        ASSERT( val->equalsInt( 42 ) );
    }

    TEST( lift_2 ) {
		auto s = R"(
					int main() {
						__sym int x;
						x %= 5;
						while( true )
							x = (x + 1) % 5;
					})";
		test_symbolic_substitution( annotation + s );
        //TODO asserts
	}

    TEST( loop_1 ) {
        auto s = R"(
                    int main() {
                        __sym int val;
						do {
						   val++;
						} while(val % 6 != 0);
                    })";
        auto m = test_symbolic_substitution( annotation + s );
        //TODO asserts
	}

    TEST( loop_2 ) {
        auto s = R"(int main() {
                        __sym int x;
                        for ( int i = 0; i < x; ++i )
                            for ( int j = 0; j < x; ++j )
                                for ( int k = 0; k < x; ++k ) {
                                    __sym int y = i * j *k;
                                }
                    })";
        auto m = test_symbolic_substitution( annotation + s );
        //TODO asserts
    }

    TEST( call_propagate_ones ) {
        auto s = R"(int call() {
                        __sym int x;
                        return x;
                    }
                    int main() {
                        int ret = call();
                        return 0;
                    })";
        auto m = test_symbolic_substitution( annotation + s );
        auto abstract_call = m->getFunction( "_Z4callv.18.19" );
        ASSERT( abstract_call );
        auto alloca = m->getFunction( "__abstract_sym_alloca" );
        ASSERT_EQ( abstract_call->getReturnType()
                 , alloca->getReturnType()->getPointerElementType() );
    }

    TEST( call_propagate_deeper_1 ) {
        auto s = R"(int call2( int x ) {
                        return x * x;
                    }
                    int call() {
                        __sym int x;
                        return x;
                    }
                    int main() {
                        int ret = call();
                        call2( ret );
                        return 0;
                    })";
        auto m = test_symbolic_substitution( annotation + s );
        //TODO asserts
    }

    TEST( call_propagate_deeper_2 ) {
        auto s = R"(
                    int call4( int x ) { return x; }
                    int call3( int x ) { return x; }
                    int call2( int x ) {
                        return call3( x ) * call4( x );
                    }
                    int call() {
                        __sym int x;
                        return x;
                    }
                    int main() {
                        int ret = call();
                        call2( ret );
                        return 0;
                    })";
        auto m = test_symbolic_substitution( annotation + s );
        //TODO asserts
    }

};

} // namespace t_abstract
#endif
} // namespace lart
