// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#ifdef BRICK_UNITTEST_REG

#include <lart/abstract/passes.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/Casting.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/query.h>
#include <lart/support/util.h>

#include <brick-llvm>
#include <brick-string>

#include <fstream>
#include <string>

#include <lart/driver.h>
#include <divine/cc/cc1.hpp>
#include <divine/cc/options.hpp>
#include <divine/rt/dios-cc.hpp>
#include <divine/rt/runtime.hpp>

namespace lart::t_abstract {

    using File = std::string;
    using Files = std::vector< File >;

    using SubstitutionPass = lart::abstract::SubstitutionPass;

    namespace rt = ::divine::rt;

    template< typename... Passes >
    auto test( std::unique_ptr< llvm::Module > m, Passes&&... passes )
    {
        using namespace abstract;
        lart::Driver drv;
        drv.setup( AnnotateFunctions(
                      "lart.interrupt.skipcfl:_ZNSt3__218uninitialized_copy.*"
                 )
                 , AnnotateFunctions(
                      "lart.interrupt.skipcfl:_ZN6__dios5Array.*"
                 )
                 , abstract::LowerAnnotations()
                 , DataFlowAnalysis()
                 , UnstashPass()
                 , Syntactic()
                 , StashPass()
                 , LowerToBool()
                 , std::forward< Passes >( passes )... );
        drv.process( m.get() );
        return m;
    }

    using namespace abstract;

    inline bool returns_abstract( llvm::Function * call ) {
        return meta::abstract::has( call->back().getTerminator() );
    }

    struct TestBase
    {
        static const bool dont_link = false;
        static const bool verbose = false;
        const std::vector< std::string > flags = { "-std=c++14" };
        std::shared_ptr< llvm::LLVMContext > _ctx;

        TestBase() : _ctx( new llvm::LLVMContext ) {}

        auto compile( const File & src )
        {
            ::divine::rt::DiosCC c( { dont_link, verbose }, _ctx );
            c.setupFS( [&]( auto yield ) { yield( "/main.cpp", src ); } );
            c.link( c.compile( "/main.cpp", flags ) );
            c.linkEntireArchive( "rst" );
            return c.takeLinked();
        }

        template< typename... Passes >
        auto test( const File & src, Passes&&... passes )
        {
            return t_abstract::test( compile( src ), std::forward< Passes >( passes )... );
        }

        template< typename... Passes >
        auto test_abstraction( const File & src, Passes&&... passes )
        {
            return test( src, std::forward< Passes >( passes )... );
        }

        template< typename... Passes >
        auto test_assume( const File & src, Passes&&... passes )
        {
            return test_abstraction( src, AddAssumes(), std::forward< Passes >( passes )... );
        }

        auto test_substitution( const File & src )
        {
            return test_assume( src, SubstitutionPass(), CallInterrupt() );
        }

        bool isVmInterupt( llvm::Function * fn )
        {
            return fn->hasName() && fn->getName() == "__dios_suspend";
        }

        const std::string annotation =
            "#define _SYM __attribute__((__annotate__(\"lart.abstract.sym\")))\n";
    };

#if 0
    struct Abstraction : TestBase
    {
        TEST( simple ) {
            auto s = "int main() { return 0; }";
            test_abstraction( annotation + s );
        }

        TEST( create ) {
            auto s = R"(int main() {
                            _SYM int abs;
                            return 0;
                        })";
            auto m = test_abstraction( annotation + s );
            auto main = m->getFunction( "main" );
            ASSERT( meta::abstract::roots( main ) );
            ASSERT_EQ( meta::enumerate( *main ).size(), 3 );
        }

        TEST( types ) {
            auto s = R"(int main() {
                            _SYM short abs_s;
                            _SYM int abs;
                            _SYM long abs_l;
                            return 0;
                        })";
            auto m = test_abstraction( annotation + s );
            auto main = m->getFunction( "main" );
            ASSERT( meta::abstract::roots( main ) );
            ASSERT_EQ( meta::enumerate( *main ).size(), 9 );
        }

        TEST( binary_ops ) {
            auto s = R"(int main() {
                            _SYM int abs;
                            int a = abs + 42;
                            int b = abs + a;
                            return 0;
                        })";
            auto m = test_abstraction( annotation + s );
            auto main = m->getFunction( "main" );
            ASSERT( meta::abstract::roots( main ) );
            ASSERT( m->getFunction( "lart.placeholder.abstract.binary.i32" ) );
        }

        TEST( phi ) {
            auto s = R"(int main() {
                            _SYM int x = 0;
                            _SYM int y = 42;
                            int z = x || y;
                            return 0;
                        })";
            auto m = test_abstraction( annotation + s );
            ASSERT( m->getFunction( "lart.placeholder.abstract.phi.i1" ) );
        }

        TEST( call_simple ) {
            auto s = R"(int call( int arg ) { return arg; }
                        int main() {
                            _SYM int abs;
                            int ret = call( abs );
                            return 0;
                        })";
            auto m = test_abstraction( annotation + s );
            auto call = m->getFunction( "_Z4calli" );
            ASSERT( meta::abstract::roots( call ) );
            ASSERT( returns_abstract( call ) );
        }

        TEST( call_twice ) {
            auto s = R"(int call( int arg ) { return arg; }
                        int main() {
                            _SYM int abs;
                            int ret = call( abs );
                            ret = call( ret );
                            return 0;
                        })";
            auto m = test_abstraction( annotation + s );
            auto call = m->getFunction( "_Z4calli" );
            ASSERT( meta::abstract::roots( call ) );
            ASSERT( returns_abstract( call ) );
        }

        TEST( call_independent ) {
            auto s = R"(int call( int arg ) { return arg; }
                        int main() {
                            _SYM int abs;
                            int ret = call( abs );
                            ret = call( 10 );
                            return 0;
                        })";
            auto m = test_abstraction( annotation + s );
            auto call = m->getFunction( "_Z4calli" );
            ASSERT( meta::abstract::roots( call ) );
            ASSERT( returns_abstract( call ) );
        }

        TEST( call_two_args_mixed ) {
            auto s = R"(int call( int a, int b ) { return a + b; }
                        int main() {
                            _SYM int abs;
                            int normal = 10;
                            int ret = call( abs, normal );
                            return 0;
                        })";
            auto m = test_abstraction( annotation + s );
            auto call = m->getFunction( "_Z4callii" );
            ASSERT( meta::abstract::roots( call ) );
            ASSERT( returns_abstract( call ) );
        }

        TEST( call_two_args_abstract ) {
            auto s = R"(int call( int a, int b ) { return a + b; }
                        int main() {
                            _SYM int a;
                            _SYM int b;
                            int ret = call( a, b );
                            return 0;
                        })";
            auto m = test_abstraction( annotation + s );
            auto call = m->getFunction( "_Z4callii" );
            ASSERT( meta::abstract::roots( call ) );
            ASSERT( returns_abstract( call ) );
        }

        TEST( call_two_args_multiple_times ) {
            auto s = R"(int call( int a, int b ) { return a + b; }
                        int main() {
                            _SYM int a;
                            _SYM int b;
                            int ret, c = 0, d = 1;
                            ret = call( a, b );
                            ret = call( a, c );
                            ret = call( c, d );
                            return 0;
                        })";
            auto m = test_abstraction( annotation + s );
            auto call = m->getFunction( "_Z4callii" );
            ASSERT( meta::abstract::roots( call ) );
            ASSERT( returns_abstract( call ) );
        }

        TEST( call_two_times_from_different_source ) {
            auto s = R"(int call1( int x ) { return x; }
                        int call2( int x ) { return call1( x ); }
                        int main() {
                            _SYM int x;
                            call2( x );
                            call1( x );
                            return 0;
                        })";
            auto m = test_abstraction( annotation + s );
            auto call1 = m->getFunction( "_Z5call1i" );
            ASSERT( meta::abstract::roots( call1 ) );
            ASSERT( returns_abstract( call1 ) );
            auto call2 = m->getFunction( "_Z5call2i" );
            ASSERT( meta::abstract::roots( call2 ) );
            ASSERT( returns_abstract( call2 ) );
        }

        TEST( propagate_from_call_not_returning_abstract ) {
            auto s = R"(int call( int x ) { return 10; }
                        int main() {
                            _SYM int x;
                            int ret = call( x );
                            ret = call( ret );
                            return 0;
                        })";
            auto m = test_abstraction( annotation + s );
            auto call = m->getFunction( "_Z4calli" );
            ASSERT( meta::abstract::roots( call ) );
            ASSERT( !returns_abstract( call ) );
        }

        TEST( call_propagate_1 ) {
            auto s = R"(int call() {
                            _SYM int x;
                            return x;
                        }
                        int main() {
                            int ret = call();
                            return 0;
                        })";
            auto m = test_abstraction( annotation + s );
            auto main = m->getFunction( "main" );
            ASSERT( meta::abstract::roots( main ) );
            auto call = m->getFunction( "_Z4callv" );
            ASSERT( meta::abstract::roots( call ) );
            ASSERT( returns_abstract( call ) );
        }

        TEST( call_propagate_back_multiple_times ) {
            auto s = R"(int nondet() {
                            _SYM int x;
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
            ASSERT( meta::abstract::roots( main ) );
            auto add = m->getFunction( "_Z3addv" );
            ASSERT( meta::abstract::roots( add ) );
            ASSERT( returns_abstract( add ) );
            auto nondet = m->getFunction( "_Z6nondetv" );
            ASSERT( meta::abstract::roots( nondet ) );
            ASSERT( returns_abstract( nondet ) );
        }

        TEST( call_propagate_deeper_1 ) {
            auto s = R"(int call2( int x ) {
                            return x * x;
                        }
                        int call1() {
                            _SYM int x;
                            return x;
                        }
                        int main() {
                            int ret = call1();
                            call2( ret );
                            return 0;
                        })";
            auto m = test_abstraction( annotation + s );
            auto main = m->getFunction( "main" );
            ASSERT( meta::abstract::roots( main ) );
            auto call1 = m->getFunction( "_Z5call1v" );
            ASSERT( meta::abstract::roots( call1 ) );
            ASSERT( returns_abstract( call1 ) );
            auto call2 = m->getFunction( "_Z5call2i" );
            ASSERT( meta::abstract::roots( call2 ) );
            ASSERT( returns_abstract( call2 ) );
        }

        TEST( call_propagate_deeper_2 ) {
            auto s = R"(
                        int call4( int x ) { return x; }
                        int call3( int x ) { return x; }
                        int call2( int x ) {
                            return call3( x ) * call4( x );
                        }
                        int call1() {
                            _SYM int x;
                            return x;
                        }
                        int main() {
                            int ret = call1();
                            call2( ret );
                            return 0;
                        })";
            auto m = test_abstraction( annotation + s );
            auto main = m->getFunction( "main" );
            ASSERT( meta::abstract::roots( main ) );
            auto call1 = m->getFunction( "_Z5call1v" );
            ASSERT( meta::abstract::roots( call1 ) );
            ASSERT( returns_abstract( call1 ) );
            auto call2 = m->getFunction( "_Z5call2i" );
            ASSERT( meta::abstract::roots( call2 ) );
            ASSERT( returns_abstract( call2 ) );
            auto call3 = m->getFunction( "_Z5call3i" );
            ASSERT( meta::abstract::roots( call3 ) );
            ASSERT( returns_abstract( call3 ) );
            auto call4 = m->getFunction( "_Z5call4i" );
            ASSERT( meta::abstract::roots( call4 ) );
            ASSERT( returns_abstract( call4 ) );
        }

        TEST( switch_test ) {
            auto s = R"(int main() {
                            _SYM int x;
                            int i = 0;
                            switch( x ) {
                                case 0: i = x; break;
                                case 1: i = (-x); break;
                                case 2: i = 42; break;
                            }
                        })";
            auto m = test_abstraction( annotation + s );
            ASSERT( m->getFunction( "lart.placeholder.abstract.freeze.i32" ) );
            ASSERT( m->getFunction( "lart.placeholder.abstract.tobool.i1" ) );
        }

        TEST( loop_test_1 ) {
            auto s = R"(int main() {
                            _SYM int x;
                            for ( int i = 0; i < x; ++i )
                                for ( int j = 0; j < x; ++j )
                                    for ( int k = 0; k < x; ++k ) {
                                        _SYM int y = i * j *k;
                                    }
                        })";
            test_abstraction( annotation + s );
        }

        TEST( loop_test_2 ) {
            auto s = R"(int main() {
                          unsigned N = 1000;
                          unsigned long long Q = 10000000000ULL;
                          unsigned long long rem = 0;
                          _SYM unsigned i;
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
            ASSERT( m->getFunction( "lart.placeholder.abstract.binary.i64" ) );
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
                            _SYM unsigned x;
                            call( x );
                            return 0;
                        })";
            auto m = test_abstraction( annotation + s );
            auto call = m->getFunction( "_Z4calli" );
            ASSERT( meta::abstract::roots( call ) );
            ASSERT( returns_abstract( call ) );
            ASSERT( m->getFunction( "lart.placeholder.abstract.binary.i32" ) );
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
                            _SYM unsigned x;
                            call( x, 100 );
                            return 0;
                        })";
            auto m = test_abstraction( annotation + s );
            auto call = m->getFunction( "_Z4callii" );
            ASSERT( meta::abstract::roots( call ) );
            ASSERT( !returns_abstract( call ) );
            ASSERT( m->getFunction( "lart.placeholder.abstract.binary.i32" ) );
        }

        TEST( recursion_without_abstract_return ) {
            auto s = R"(
                        int call( int x, int y ) {
                            if ( y == 0 || x == 0 )
                                return x;
                            else return call(x - 1, y - 1);
                        }
                        int main() {
                            _SYM unsigned x;
                            call( 100, x );
                            return 0;
                        })";
            auto m = test_abstraction( annotation + s );
            auto call = m->getFunction( "_Z4callii" );
            ASSERT( meta::abstract::roots( call ) );
            ASSERT( returns_abstract( call ) );
            ASSERT( m->getFunction( "lart.placeholder.abstract.binary.i32" ) );
        }

        TEST( struct_simple ) {
            auto s = R"(struct S { int x; };

                        int main() {
                            _SYM int x;
                            S s;
                            s.x = x;
                        })";
            auto m = test_abstraction( annotation + s );
            auto main = m->getFunction( "main" );
            ASSERT( meta::abstract::roots( main ) );
        }

        TEST( struct_multiple_accesses ) {
            auto s = R"(struct S { int x; };

                        int main() {
                            _SYM int x;
                            S s;
                            s.x = x;
                            if ( s.x == 0 )
                                return 0;
                            else
                                return 1;
                        })";
            auto m = test_abstraction( annotation + s );
            auto main = m->getFunction( "main" );
            ASSERT( meta::abstract::roots( main ) );
            ASSERT( m->getFunction( "lart.placeholder.abstract.cmp.i1" ) );
        }

        TEST( struct_complex ) {
            auto s = R"(struct S { int x, y, z; };

                        int main() {
                            _SYM int x;
                            S s;
                            s.y = x;
                            return s.y;
                        })";
            auto m = test_abstraction( annotation + s );
            auto main = m->getFunction( "main" );
            ASSERT( meta::abstract::roots( main ) );
            ASSERT( returns_abstract( main ) );
        }

        TEST( struct_multiple_abstract_values ) {
            auto s = R"(struct S { int x, y, z; };

                        int main() {
                            _SYM int x;
                            _SYM int y;
                            S s;
                            s.x = x;
                            s.y = y;
                            s.z = 42;
                            return s.x;
                        })";
            auto m = test_abstraction( annotation + s );
            auto main = m->getFunction( "main" );
            ASSERT( meta::abstract::roots( main ) );
            ASSERT( returns_abstract( main ) );
        }

        TEST( struct_nested_1 ) {
            auto s = R"(struct S { int x; };
                        struct T { S a; S b; };

                        int main() {
                            _SYM int x;
                            T t;
                            t.a.x = x;
                            return t.a.x;
                        })";
            auto m = test_abstraction( annotation + s );
            auto main = m->getFunction( "main" );
            ASSERT( meta::abstract::roots( main ) );
            ASSERT( returns_abstract( main ) );
        }

        TEST( struct_nested_2 ) {
            auto s = R"(struct S { int x; };
                        struct U { S s; };
                        struct V { U u; };

                        int main() {
                            _SYM int x;
                            V v;
                            v.u.s.x = x;
                            return v.u.s.x;
                        })";
            auto m = test_abstraction( annotation + s );
            auto main = m->getFunction( "main" );
            ASSERT( meta::abstract::roots( main ) );
            ASSERT( returns_abstract( main ) );
        }

        TEST( output_int_arg_1 ) {
            auto s = R"(void init( int * i ) {
                            _SYM int v;
                            *i = v;
                        }
                        int main() {
                            int i;
                            init( &i );
                            return i;
                        })";
            auto m = test_abstraction( annotation + s );
            auto main = m->getFunction( "main" );
            ASSERT( meta::abstract::roots( main ) );
            ASSERT( returns_abstract( main ) );
            auto init = m->getFunction( "_Z4initPi" );
            ASSERT( meta::abstract::roots( init ) );
        }

        TEST( output_int_arg_2 ) {
            auto s = R"(void init( int * i, int v ) {
                            *i = v;
                        }
                        int main() {
                            int i;
                            _SYM int v;
                            init( &i, v );
                            return i;
                        })";
            auto m = test_abstraction( annotation + s );
            auto main = m->getFunction( "main" );
            auto init = m->getFunction( "_Z4initPii" );
            ASSERT( meta::abstract::roots( main ) );
            ASSERT( returns_abstract( main ) );
            ASSERT( meta::abstract::roots( init ) );
        }

        TEST( output_int_arg_3 ) {
            auto s = R"(
                        void init_impl( int * i ) {
                            _SYM int v;
                            *i = v;
                        }
                        void init( int * i ) {
                            init_impl( i );
                        }
                        int main() {
                            int i;
                            init( &i );
                            int v = i;
                            return v;
                        })";
            auto m = test_abstraction( annotation + s );
            auto main = m->getFunction( "main" );
            auto init = m->getFunction( "_Z4initPi" );
            auto init_impl = m->getFunction( "_Z9init_implPi" );

            ASSERT( meta::abstract::roots( main ) );
            ASSERT( returns_abstract( main ) );
            ASSERT( meta::abstract::roots( init ) );
            ASSERT( meta::abstract::roots( init_impl ) );
        }

        TEST( output_int_arg_4 ) {
            auto s = R"(
                        void init_impl( int * i, int v ) {
                            *i = v;
                        }
                        void init( int * i ) {
                            _SYM int v;
                            init_impl( i, v );
                        }
                        int main() {
                            int i;
                            init( &i );
                            return i;
                        })";
            auto m = test_abstraction( annotation + s );
            auto main = m->getFunction( "main" );
            auto init = m->getFunction( "_Z4initPi" );
            auto init_impl = m->getFunction( "_Z9init_implPii" );
            ASSERT( meta::abstract::roots( main ) );
            ASSERT( returns_abstract( main ) );
            ASSERT( meta::abstract::roots( init ) );
            ASSERT( meta::abstract::roots( init_impl ) );
        }

        TEST( output_struct_arg_1 ) {
            auto s = R"(struct Widget{ int i; };
                        void init( Widget * w ) {
                            _SYM int v;
                            w->i = v;
                        }
                        int main() {
                            Widget w;
                            init( &w );
                            return w.i;
                        })";
            auto m = test_abstraction( annotation + s );
            auto main = m->getFunction( "main" );
            auto init = m->getFunction( "_Z4initP6Widget" );
            ASSERT( meta::abstract::roots( main ) );
            ASSERT( returns_abstract( main ) );
            ASSERT( meta::abstract::roots( init ) );
        }

        TEST( output_struct_arg_2 ) {
            auto s = R"(struct Widget{ int i; };
                        void init( Widget * w ) {
                            _SYM int v;
                            w->i = v % 10;
                        }

                        bool check( Widget * w ) {
                            return w->i < 10;
                        }
                        int main() {
                            Widget w;
                            init( &w );
                            return check( &w );
                        })";
            auto m = test_abstraction( annotation + s );
            auto main = m->getFunction( "main" );
            auto init = m->getFunction( "_Z4initP6Widget" );
            auto check = m->getFunction( "_Z5checkP6Widget" );
            ASSERT( meta::abstract::roots( main ) );
            ASSERT( returns_abstract( main ) );
            ASSERT( meta::abstract::roots( init ) );
            ASSERT( meta::abstract::roots( check ) );
            ASSERT( returns_abstract( check ) );
        }

        TEST( output_struct_arg_3 ) {
            auto s = R"(struct Widget{ int i; };
                        struct Store{ Widget * w; };
                        void init( Widget * w ) {
                            _SYM int v;
                            w->i = v % 10;
                        }

                        void init( Store * s, Widget * w ) {
                            s->w = w;
                        }
                        int main() {
                            Store store;
                            Widget w;
                            init( &w );
                            init( &store, &w  );
                            return store.w->i;
                        })";
            auto m = test_abstraction( annotation + s );
            auto main = m->getFunction( "main" );
            auto init = m->getFunction( "_Z4initP6Widget" );
            auto init_store = m->getFunction( "_Z4initP5StoreP6Widget" );
            ASSERT( meta::abstract::roots( main ) );
            ASSERT( returns_abstract( main ) );
            ASSERT( meta::abstract::roots( init ) );
            ASSERT( meta::abstract::roots( init_store ) );
        }

        TEST( global_1 ) {
            auto s = R"(int g;

                        int f() { return g; }

                        int main() {
                            _SYM int v;
                            g = v;
                            return f();
                        })";
            auto m = test_abstraction( annotation + s );
            auto main = m->getFunction( "main" );
            auto f = m->getFunction( "_Z1fv" );
            ASSERT( meta::abstract::roots( main ) );
            ASSERT( returns_abstract( main ) );
            ASSERT( meta::abstract::roots( f ) );
            ASSERT( returns_abstract( f ) );
        }
    };

    struct Assume : TestBase
    {
        TEST( simple ) {
            auto s = "int main() { return 0; }";
            test_assume( annotation + s );
        }

        TEST( tristate ) {
            auto s = R"(int main() {
                            _SYM int x;
                            if ( x > 0 )
                                return 42;
                            else
                                return 0;
                        })";
            test_assume( annotation + s );
        }

        TEST( loop ) {
            auto s = R"(int main() {
                            _SYM int abs;
                            while( abs ) {
                                ++abs;
                            }
                            return 0;
                        })";
            test_assume( annotation + s );
        }
    };

    struct Substitution : TestBase
    {
        TEST( simple ) {
            auto s = "int main() { return 0; }";
            test_substitution( annotation + s );
        }

        TEST( create ) {
            auto s = R"(int main() {
                            _SYM int abs;
                            return abs;
                        })";
            auto m = test_substitution( annotation + s );
            auto main = m->getFunction( "main" );
            ASSERT( meta::abstract::roots( main ) );
            ASSERT( returns_abstract( main ) );
        }

        TEST( types ) {
            auto s = R"(int main() {
                            _SYM short abs_s;
                            _SYM int abs;
                            _SYM long abs_l;
                            return 0;
                        })";
            auto m = test_substitution( annotation + s );
            auto main = m->getFunction( "main" );
            ASSERT( meta::abstract::roots( main ) );
        }

        TEST( binary_ops ) {
            auto s = R"(int main() {
                            _SYM int abs;
                            int a = abs + 42;
                            int b = abs + a;
                            return 0;
                        })";
            auto m = test_substitution( annotation + s );
            auto main = m->getFunction( "main" );
            ASSERT( meta::abstract::roots( main ) );
            ASSERT( m->getFunction( "__sym_lift" ) );
            ASSERT( m->getFunction( "__sym_add" ) );
        }

        TEST( tristate ) {
            auto s = R"(int main() {
                            _SYM int x;
                            if ( x > 0 )
                                return 42;
                            else
                                return 0;
                        })";
            auto m = test_substitution( annotation + s );
            auto main = m->getFunction( "main" );
            ASSERT( meta::abstract::roots( main ) );
        }

        TEST( phi ) {
            auto s = R"(int main() {
                            _SYM int x = 0;
                            _SYM int y = 42;
                            int z = x || y;
                            return z;
                        })";
            auto m = test_substitution( annotation + s );
            auto main = m->getFunction( "main" );
            ASSERT( meta::abstract::roots( main ) );
            ASSERT( returns_abstract( main ) );
        }

        TEST( switch_test ) {
            auto s = R"(int main() {
                            _SYM int x;
                            int i = 0;
                            switch( x ) {
                                case 0: i = x; break;
                                case 1: i = (-x); break;
                                case 2: i = 42; break;
                            }
                        })";
            auto m = test_substitution( annotation + s );
            auto main = m->getFunction( "main" );
            ASSERT( meta::abstract::roots( main ) );
        }

        TEST( loop_1 ) {
            auto s = R"(
                        int main() {
                            _SYM int val;
                            do {
                               val++;
                            } while(val % 6 != 0);
                        })";
            auto m = test_substitution( annotation + s );
            auto main = m->getFunction( "main" );
            ASSERT( meta::abstract::roots( main ) );
        }

        TEST( loop_2 ) {
            auto s = R"(int main() {
                            _SYM int x;
                            for ( int i = 0; i < x; ++i )
                                for ( int j = 0; j < x; ++j )
                                    for ( int k = 0; k < x; ++k ) {
                                        _SYM int y = i * j *k;
                                    }
                        })";
            auto m = test_substitution( annotation + s );
            auto main = m->getFunction( "main" );
            ASSERT( meta::abstract::roots( main ) );
        }

        TEST( call_propagate_ones )
        {
            auto s = R"(int call() {
                            _SYM int x;
                            return x;
                        }
                        int main() {
                            int ret = call();
                            return 0;
                        })";
            auto m = test_substitution( annotation + s );
            auto main = m->getFunction( "main" );
            ASSERT( meta::abstract::roots( main ) );
            auto call = m->getFunction( "_Z4callv" );
            ASSERT( meta::abstract::roots( call ) );
            ASSERT( returns_abstract( call ) );
        }

        TEST( call_propagate_deeper_1 ) {
            auto s = R"(int call2( int x ) {
                            return x * x;
                        }
                        int call1() {
                            _SYM int x;
                            return x;
                        }
                        int main() {
                            int ret = call1();
                            call2( ret );
                            return 0;
                        })";
            auto m = test_substitution( annotation + s );
            auto main = m->getFunction( "main" );
            ASSERT( meta::abstract::roots( main ) );
            auto call1 = m->getFunction( "_Z5call1v" );
            ASSERT( meta::abstract::roots( call1 ) );
            ASSERT( returns_abstract( call1 ) );
            auto call2 = m->getFunction( "_Z5call2i" );
            ASSERT( meta::abstract::roots( call2 ) );
            ASSERT( returns_abstract( call2 ) );
        }

        TEST( call_propagate_deeper_2 ) {
            auto s = R"(
                        int call4( int x ) { return x; }
                        int call3( int x ) { return x; }
                        int call2( int x ) {
                            return call3( x ) * call4( x );
                        }
                        int call1() {
                            _SYM int x;
                            return x;
                        }
                        int main() {
                            int ret = call1();
                            call2( ret );
                            return 0;
                        })";
            auto m = test_substitution( annotation + s );
            auto main = m->getFunction( "main" );
            ASSERT( meta::abstract::roots( main ) );
            auto call1 = m->getFunction( "_Z5call1v" );
            ASSERT( meta::abstract::roots( call1 ) );
            ASSERT( returns_abstract( call1 ) );
            auto call2 = m->getFunction( "_Z5call2i" );
            ASSERT( meta::abstract::roots( call2 ) );
            ASSERT( returns_abstract( call2 ) );
            auto call3 = m->getFunction( "_Z5call3i" );
            ASSERT( meta::abstract::roots( call3 ) );
            ASSERT( returns_abstract( call3 ) );
            auto call4 = m->getFunction( "_Z5call4i" );
            ASSERT( meta::abstract::roots( call4 ) );
            ASSERT( returns_abstract( call4 ) );
        }

        TEST( call_undef_return ) {
            auto s = R"(
                        bool is_zero( int v ) { return v == 0; }
                        int main() {
                            _SYM int input;
                            return is_zero( input );
                        })";
            test_substitution( annotation + s );
        }
    };
#endif

} // namespace lart::t_abstract
#endif
