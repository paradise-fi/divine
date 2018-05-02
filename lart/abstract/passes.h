// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Support/Casting.h>
DIVINE_UNRELAX_WARNINGS

#include <divine/cc/clang.hpp>
#include <divine/cc/compile.hpp>
#include <divine/cc/options.hpp>
#include <divine/rt/runtime.hpp>

#include <lart/support/meta.h>
#include <lart/support/query.h>
#include <lart/support/util.h>

#include <brick-llvm>
#include <brick-string>

#include <fstream>
#include <string>

#include <lart/abstract/metadata.h>
#include <lart/abstract/vpa.h>
#include <lart/abstract/duplicator.h>
#include <lart/abstract/branching.h>
#include <lart/abstract/stash.h>
#include <lart/abstract/assume.h>
#include <lart/abstract/bcp.h>
#include <lart/abstract/substitution.h>
#include <lart/abstract/interrupt.h>

namespace lart {
namespace abstract {

    template< typename Passes >
    struct PassWrapperImpl {
        PassWrapperImpl( Passes&& passes ) : passes( std::move( passes ) ) {}

        void run( llvm::Module & m ) {
            apply( [&]( auto... p ) { ( p.run( m ),... ); }, passes );
        }

        Passes passes;
    };

    template < typename... Passes >
    auto make_pass_wrapper( Passes... passes ) {
        using passes_t = std::tuple< Passes... >;
        return PassWrapperImpl< passes_t >( std::forward_as_tuple( passes... ) );
    }

    struct PassWrapper {
        static PassMeta meta() {
            return passMeta< PassWrapper >(
                "Abstraction", "Abstract annotated values to given domains." );
        }

        void run( llvm::Module & m ) {
            auto passes = make_pass_wrapper( CreateAbstractMetadata()
                                           , VPA()
                                           , Duplicator()
                                           , Stash()
                                           , ExpandBranching()
                                           , AddAssumes()
                                           , InDomainDuplicate()
                                           , UnrepStores()
                                           , Tainting()
                                           , Synthesize()
                                           , CallInterrupt()
										   );
            passes.run( m );
        }

    };

    inline std::vector< PassMeta > passes() {
        return { PassWrapper::meta() };
    }
}

#ifdef BRICK_UNITTEST_REG
#include <lart/driver.h>

namespace t_abstract {

using File = std::string;
using Files = std::vector< File >;

using Compile = divine::cc::Compile;

void mapVirtualFile( Compile & c, const File & path, const File & src ) {
    c.setupFS( [&]( std::function< void( std::string, const std::string & ) > yield ) {
        yield( path, src );
    } );
}

namespace rt = divine::rt;

std::string source( const File & name ) {
    std::string res;
    rt::each( [&]( auto path, auto src ) {
        if ( brick::string::endsWith( path, name ) )
            res = std::string( src );
    } );
    return res;
}

void setupFS( Compile & c ) {
	auto each = [&]( auto filter ) {
        return [&filter]( std::function< void( std::string, std::string_view ) > yield ) {
            rt::each( filter( yield ) );
        };
    };

    c.setupFS( each( [&]( std::function< void( std::string, std::string_view ) > yield ) {
        return [&]( auto path, auto src ) {
			if ( !brick::string::endsWith( path, ".bc" ) )
				yield( path, src );
		};
	} ) );
}

struct TestCompile {
    static const bool dont_link = false;
    static const bool verbose = false;
    const std::vector< std::string > flags = { "-std=c++14" };

    using Linker = brick::llvm::Linker;

    TestCompile() : cmp( { dont_link, verbose }, std::make_shared< llvm::LLVMContext >() )
    {
        setupFS( cmp );
    }

    TestCompile( const Files & link, const Files & hdrs ) : TestCompile() {
        for ( const auto & f : link )
            mapVirtualFile( cmp, f, source( f ) );
        for ( const auto & f : hdrs )
            mapVirtualFile( cmp, f, source( f ) );
        for ( const auto & f : link )
            linker.link( cmp.compile( f, flags ) );
    }

    auto compile( const File & src ) {
        mapVirtualFile( cmp, "main.cpp", src );
        Linker l;
        if ( linker.hasModule() )
            l.link( (*linker.get()) );
        l.link( cmp.compile( "main.cpp", flags ) );
        return l.take();
    }

private:
    Linker linker;
    Compile cmp;
};

static TestCompile cmp( { "sym.cpp", "tristate.cpp", "formula.cpp" },
                        { "domain.def", "sym.h", "tristate.h", "common.h", "formula.h" } );

std::string load( const File & path ) {
    std::ifstream file( path );
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

template< typename... Passes >
auto test( Compile::ModulePtr m, Passes&&... passes ) {
    using namespace abstract;
    lart::Driver drv;
    drv.setup( CreateAbstractMetadata()
             , VPA()
             , Duplicator()
             , Stash()
             , ExpandBranching()
             , std::forward< Passes >( passes )... );
    drv.process( m.get() );
    return m;
}

template< typename... Passes >
auto test( TestCompile & c, const File & src, Passes&&... passes ) {
    return test( c.compile( src ), std::forward< Passes >( passes )... );
}

template< typename... Passes >
auto test_abstraction( const File & src, Passes&&... passes ) {
    using namespace abstract;
    return test( cmp, src, std::forward< Passes >( passes )... );
}

template< typename... Passes >
auto test_assume( const File & src, Passes&&... passes ) {
    using namespace abstract;
    return test_abstraction(src, AddAssumes(), std::forward< Passes >( passes )...);
}

auto test_substitution( const File & src ) {
    using namespace abstract;
    return test_assume( src
                      , InDomainDuplicate()
                      , Tainting()
                      , UnrepStores()
                      , Synthesize()
                      , CallInterrupt() );
}

static const std::string annotation =
    "#define _SYM __attribute__((__annotate__(\"lart.abstract.sym\")))\n";

bool isVmInterupt( llvm::Function * fn )
{
    return fn->hasName() && fn->getName() == "__dios_suspend";
}

using namespace abstract;

struct Abstraction {
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
        ASSERT( main->getMetadata( "lart.abstract.roots" ) );
        ASSERT_EQ( abstract_metadata( main ).size(), 3 );
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
        ASSERT( main->getMetadata( "lart.abstract.roots" ) );
        ASSERT_EQ( abstract_metadata( main ).size(), 9 );
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
        ASSERT( main->getMetadata( "lart.abstract.roots" ) );
        ASSERT( m->getFunction( "lart.placeholder.lart.sym.i32" ) );
    }

    TEST( phi ) {
        auto s = R"(int main() {
                        _SYM int x = 0;
                        _SYM int y = 42;
                        int z = x || y;
                        return 0;
                    })";
        auto m = test_abstraction( annotation + s );
        ASSERT( m->getFunction( "lart.placeholder.lart.sym.i32" ) );
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
        ASSERT( call->getMetadata( "lart.abstract.roots" ) );
        ASSERT( call->back().getTerminator()->getMetadata( "lart.domains" ) );
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
        ASSERT( call->getMetadata( "lart.abstract.roots" ) );
        ASSERT( call->back().getTerminator()->getMetadata( "lart.domains" ) );
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
        ASSERT( call->getMetadata( "lart.abstract.roots" ) );
        ASSERT( call->back().getTerminator()->getMetadata( "lart.domains" ) );
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
        ASSERT( call->getMetadata( "lart.abstract.roots" ) );
        ASSERT( call->back().getTerminator()->getMetadata( "lart.domains" ) );
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
        ASSERT( call->getMetadata( "lart.abstract.roots" ) );
        ASSERT( call->back().getTerminator()->getMetadata( "lart.domains" ) );
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
        ASSERT( call->getMetadata( "lart.abstract.roots" ) );
        ASSERT( call->back().getTerminator()->getMetadata( "lart.domains" ) );
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
        ASSERT( call1->getMetadata( "lart.abstract.roots" ) );
        ASSERT( call1->back().getTerminator()->getMetadata( "lart.domains" ) );
        auto call2 = m->getFunction( "_Z5call2i" );
        ASSERT( call2->getMetadata( "lart.abstract.roots" ) );
        ASSERT( call2->back().getTerminator()->getMetadata( "lart.domains" ) );
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
        ASSERT( call->getMetadata( "lart.abstract.roots" ) );
        ASSERT( !call->back().getTerminator()->getMetadata( "lart.domains" ) );
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
        ASSERT( main->getMetadata( "lart.abstract.roots" ) );
        auto call = m->getFunction( "_Z4callv" );
        ASSERT( call->getMetadata( "lart.abstract.roots" ) );
        ASSERT( call->back().getTerminator()->getMetadata( "lart.domains" ) );
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
        ASSERT( main->getMetadata( "lart.abstract.roots" ) );
        auto add = m->getFunction( "_Z3addv" );
        ASSERT( add->getMetadata( "lart.abstract.roots" ) );
        ASSERT( add->back().getTerminator()->getMetadata( "lart.domains" ) );
        auto nondet = m->getFunction( "_Z6nondetv" );
        ASSERT( nondet->getMetadata( "lart.abstract.roots" ) );
        ASSERT( nondet->back().getTerminator()->getMetadata( "lart.domains" ) );
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
        ASSERT( main->getMetadata( "lart.abstract.roots" ) );
        auto call1 = m->getFunction( "_Z5call1v" );
        ASSERT( call1->getMetadata( "lart.abstract.roots" ) );
        ASSERT( call1->back().getTerminator()->getMetadata( "lart.domains" ) );
        auto call2 = m->getFunction( "_Z5call2i" );
        ASSERT( call2->getMetadata( "lart.abstract.roots" ) );
        ASSERT( call2->back().getTerminator()->getMetadata( "lart.domains" ) );
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
        ASSERT( main->getMetadata( "lart.abstract.roots" ) );
        auto call1 = m->getFunction( "_Z5call1v" );
        ASSERT( call1->getMetadata( "lart.abstract.roots" ) );
        ASSERT( call1->back().getTerminator()->getMetadata( "lart.domains" ) );
        auto call2 = m->getFunction( "_Z5call2i" );
        ASSERT( call2->getMetadata( "lart.abstract.roots" ) );
        ASSERT( call2->back().getTerminator()->getMetadata( "lart.domains" ) );
        auto call3 = m->getFunction( "_Z5call3i" );
        ASSERT( call3->getMetadata( "lart.abstract.roots" ) );
        ASSERT( call3->back().getTerminator()->getMetadata( "lart.domains" ) );
        auto call4 = m->getFunction( "_Z5call4i" );
        ASSERT( call4->getMetadata( "lart.abstract.roots" ) );
        ASSERT( call4->back().getTerminator()->getMetadata( "lart.domains" ) );
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
        ASSERT( m->getFunction( "lart.placeholder.lart.sym.i32" ) );
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
        auto m = test_abstraction( annotation + s );
        ASSERT( m->getFunction( "lart.placeholder.lart.sym.i32" ) );
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
        ASSERT( m->getFunction( "lart.placeholder.lart.sym.i64" ) );
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
        ASSERT( call->getMetadata( "lart.abstract.roots" ) );
        ASSERT( call->back().getTerminator()->getMetadata( "lart.domains" ) );
        ASSERT( m->getFunction( "lart.placeholder.lart.sym.i32" ) );
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
        ASSERT( call->getMetadata( "lart.abstract.roots" ) );
        ASSERT( !call->back().getTerminator()->getMetadata( "lart.domains" ) );
        ASSERT( m->getFunction( "lart.placeholder.lart.sym.i32" ) );
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
        ASSERT( call->getMetadata( "lart.abstract.roots" ) );
        ASSERT( call->back().getTerminator()->getMetadata( "lart.domains" ) );
        ASSERT( m->getFunction( "lart.placeholder.lart.sym.i32" ) );
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
        ASSERT( main->getMetadata( "lart.abstract.roots" ) );
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
        ASSERT( main->getMetadata( "lart.abstract.roots" ) );
        ASSERT( m->getFunction( "lart.placeholder.lart.sym.i32" ) );
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
        ASSERT( main->getMetadata( "lart.abstract.roots" ) );
        ASSERT( main->back().getTerminator()->getMetadata( "lart.domains" ) );
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
        ASSERT( main->getMetadata( "lart.abstract.roots" ) );
        ASSERT( main->back().getTerminator()->getMetadata( "lart.domains" ) );
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
        ASSERT( main->getMetadata( "lart.abstract.roots" ) );
        ASSERT( main->back().getTerminator()->getMetadata( "lart.domains" ) );
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
        ASSERT( main->getMetadata( "lart.abstract.roots" ) );
        ASSERT( main->back().getTerminator()->getMetadata( "lart.domains" ) );
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
        ASSERT( main->getMetadata( "lart.abstract.roots" ) );
        ASSERT( main->back().getTerminator()->getMetadata( "lart.domains" ) );
        auto init = m->getFunction( "_Z4initPi" );
        ASSERT( init->getMetadata( "lart.abstract.roots" ) );
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
        ASSERT( main->getMetadata( "lart.abstract.roots" ) );
        ASSERT( main->back().getTerminator()->getMetadata( "lart.domains" ) );
        ASSERT( init->getMetadata( "lart.abstract.roots" ) );
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

        ASSERT( main->getMetadata( "lart.abstract.roots" ) );
        ASSERT( main->back().getTerminator()->getMetadata( "lart.domains" ) );
        ASSERT( init->getMetadata( "lart.abstract.roots" ) );
        ASSERT( init_impl->getMetadata( "lart.abstract.roots" ) );
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
        ASSERT( main->getMetadata( "lart.abstract.roots" ) );
        ASSERT( main->back().getTerminator()->getMetadata( "lart.domains" ) );
        ASSERT( init->getMetadata( "lart.abstract.roots" ) );
        ASSERT( init_impl->getMetadata( "lart.abstract.roots" ) );
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
        ASSERT( main->getMetadata( "lart.abstract.roots" ) );
        ASSERT( main->back().getTerminator()->getMetadata( "lart.domains" ) );
        ASSERT( init->getMetadata( "lart.abstract.roots" ) );
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
        ASSERT( main->getMetadata( "lart.abstract.roots" ) );
        ASSERT( main->back().getTerminator()->getMetadata( "lart.domains" ) );
        ASSERT( init->getMetadata( "lart.abstract.roots" ) );
        ASSERT( check->getMetadata( "lart.abstract.roots" ) );
        ASSERT( check->back().getTerminator()->getMetadata( "lart.domains" ) );
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
        ASSERT( main->getMetadata( "lart.abstract.roots" ) );
        ASSERT( main->back().getTerminator()->getMetadata( "lart.domains" ) );
        ASSERT( init->getMetadata( "lart.abstract.roots" ) );
        ASSERT( init_store->getMetadata( "lart.abstract.roots" ) );
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
        ASSERT( main->getMetadata( "lart.abstract.roots" ) );
        ASSERT( main->back().getTerminator()->getMetadata( "lart.domains" ) );
        ASSERT( f->getMetadata( "lart.abstract.roots" ) );
        ASSERT( f->back().getTerminator()->getMetadata( "lart.domains" ) );
    }
};

struct Assume {
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

struct Substitution {
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
        ASSERT( main->getMetadata( "lart.abstract.roots" ) );
        ASSERT( main->back().getTerminator()->getMetadata( "lart.domains" ) );
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
        ASSERT( main->getMetadata( "lart.abstract.roots" ) );
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
        ASSERT( main->getMetadata( "lart.abstract.roots" ) );
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
        ASSERT( main->getMetadata( "lart.abstract.roots" ) );
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
        ASSERT( main->getMetadata( "lart.abstract.roots" ) );
        ASSERT( main->back().getTerminator()->getMetadata( "lart.domains" ) );
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
        ASSERT( main->getMetadata( "lart.abstract.roots" ) );
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
        ASSERT( main->getMetadata( "lart.abstract.roots" ) );
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
        ASSERT( main->getMetadata( "lart.abstract.roots" ) );
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
        ASSERT( main->getMetadata( "lart.abstract.roots" ) );
        auto call = m->getFunction( "_Z4callv" );
        ASSERT( call->getMetadata( "lart.abstract.roots" ) );
        ASSERT( call->back().getTerminator()->getMetadata( "lart.domains" ) );
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
        ASSERT( main->getMetadata( "lart.abstract.roots" ) );
        auto call1 = m->getFunction( "_Z5call1v" );
        ASSERT( call1->getMetadata( "lart.abstract.roots" ) );
        ASSERT( call1->back().getTerminator()->getMetadata( "lart.domains" ) );
        auto call2 = m->getFunction( "_Z5call2i" );
        ASSERT( call2->getMetadata( "lart.abstract.roots" ) );
        ASSERT( call2->back().getTerminator()->getMetadata( "lart.domains" ) );
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
        ASSERT( main->getMetadata( "lart.abstract.roots" ) );
        auto call1 = m->getFunction( "_Z5call1v" );
        ASSERT( call1->getMetadata( "lart.abstract.roots" ) );
        ASSERT( call1->back().getTerminator()->getMetadata( "lart.domains" ) );
        auto call2 = m->getFunction( "_Z5call2i" );
        ASSERT( call2->getMetadata( "lart.abstract.roots" ) );
        ASSERT( call2->back().getTerminator()->getMetadata( "lart.domains" ) );
        auto call3 = m->getFunction( "_Z5call3i" );
        ASSERT( call3->getMetadata( "lart.abstract.roots" ) );
        ASSERT( call3->back().getTerminator()->getMetadata( "lart.domains" ) );
        auto call4 = m->getFunction( "_Z5call4i" );
        ASSERT( call4->getMetadata( "lart.abstract.roots" ) );
        ASSERT( call4->back().getTerminator()->getMetadata( "lart.domains" ) );
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

} // namespace t_abstract
#endif
} // namespace lart
