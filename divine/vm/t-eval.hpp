// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2012-2018 Petr Ročkai <code@fixp.eu>
 * (c) 2015 Vladimír Štill
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#pragma once

#include <divine/vm/eval.tpp>
#include <divine/vm/memory.hpp>

namespace divine::t_vm
{

using vm::CodePointer;
namespace Intrinsic = ::llvm::Intrinsic;

template< typename Prog >
struct TContext : vm::Context< Prog, vm::SmallHeap >
{
    using Super = vm::Context< Prog, vm::SmallHeap >;
    vm::Fault _fault;

    void fault( vm::Fault f, vm::HeapPointer, CodePointer )
    {
        _fault = f;
        this->set( _VM_CR_Frame, vm::nullPointer() );
    }

    using Super::trace;
    void trace( std::string s ) { std::cerr << "T: " << s << std::endl; }
    void trace( vm::TraceDebugPersist ) { UNREACHABLE( "debug persist not allowed in unit tests" ); }

    TContext( Prog &p ) : vm::Context< Prog, vm::SmallHeap >( p ), _fault( _VM_F_NoFault ) {}
};

#ifdef BRICK_UNITTEST_REG
struct Eval
{
    using IntV = vm::value::Int< 32 >;

    template< typename... Args >
    auto testP( std::shared_ptr< vm::Program > p, Args... args )
    {
        TContext< vm::Program > c( *p );
        auto data = p->exportHeap( c.heap() );
        c.set( _VM_CR_Constants, data.first );
        c.set( _VM_CR_Globals , data.second );
        vm::Eval< TContext< vm::Program > > e( c );
        auto pc = p->functionByName( "f" );
        c.enter( pc, vm::nullPointerV(), args... );
        c.set( _VM_CR_Flags, _VM_CF_KernelMode | _VM_CF_AutoSuspend );
        e.run();
        return e.retval< IntV >();
    }

    template< typename... Args >
    int testF( std::string s, Args... args )
    {
        return testP( c2prog( s ), args... ).cooked();
    }

    template< typename Build, typename... Args >
    int testLLVM( Build build, llvm::FunctionType *ft = nullptr, Args... args )
    {
        return testP( ir2prog( build, "f", ft ), args... ).cooked();
    }

    TEST(simple)
    {
        int x = testF( "int f( int a, int b ) { return a + b; }",
                       IntV( 10 ), IntV( 20 ) );
        ASSERT_EQ( x, 30 );
    }

    TEST(call_0)
    {
        int x = testF( "int g() { return 1; } int f( int a ) { return g() + a; }",
                       IntV( 10 ) );
        ASSERT_EQ( x, 11 );
    }

    TEST(call_1)
    {
        int x = testF( "void g(int b) { b = b+1; } int f( int a ) { g(a); return a; }",
                       IntV( 10 ) );
        ASSERT_EQ( x, 10 );
    }

    TEST(call_2)
    {
        int x = testF( "void g(int *b) { *b = (*b)+1; } int f( int a ) { g(&a); return a; }",
                       IntV( 10 ) );
        ASSERT_EQ( x, 11 );
    }

    TEST(ptr)
    {
        int x = testF( "void *__vm_obj_make( int ); int __vm_obj_size( void * ); "
                       "int f() { void *x = __vm_obj_make( 10 );"
                       "return __vm_obj_size( x ); }" );
        ASSERT_EQ( x, 10 );
    }

    TEST(array_1g)
    {
        auto f = [this]( int i ) {
            return testF( "int array[8] = { 0, 1, 2, 3 }; "
                          "int f() { return array[ " + std::to_string( i ) + " ]; }" );
        };
        ASSERT_EQ( f( 0 ), 0 );
        ASSERT_EQ( f( 1 ), 1 );
        ASSERT_EQ( f( 2 ), 2 );
        ASSERT_EQ( f( 3 ), 3 );
        ASSERT_EQ( f( 4 ), 0 );
        ASSERT_EQ( f( 7 ), 0 );
    }

    TEST(array_2g)
    {
        auto f = [this]( int i ) {
            return testF( "int array[8] = { 0 }; "
                          "int f() { for ( int i = 0; i < 4; ++i "
                          ") array[i] = i; return array[ " + std::to_string( i ) + " ]; }" );
        };
        ASSERT_EQ( f( 0 ), 0 );
        ASSERT_EQ( f( 1 ), 1 );
        ASSERT_EQ( f( 2 ), 2 );
        ASSERT_EQ( f( 3 ), 3 );
        ASSERT_EQ( f( 4 ), 0 );
        ASSERT_EQ( f( 7 ), 0 );
    }

    TEST(array_3)
    {
        // note can't use unitializer, requires memcpy
        auto f = [this]( int i ) {
            return testF( "int f() { int array[4]; "
                          "for ( int i = 0; i < 4; ++i ) array[i] = i; "
                          "return array[ " + std::to_string( i ) + " ]; }" );
        };
        ASSERT_EQ( f( 0 ), 0 );
        ASSERT_EQ( f( 1 ), 1 );
        ASSERT_EQ( f( 2 ), 2 );
        ASSERT_EQ( f( 3 ), 3 );
    }

    TEST(ptrarith)
    {
        const char *fdef = R"|(void __vm_trace( int, const char *v );
                               int r = 1;
                               void fail( const char *v ) { __vm_trace( 0, v ); r = 0; }
                               int f() {
                                   char array[ 2 ];
                                   char *p = array;
                                   unsigned long ulong = (unsigned long)p;
                                   if ( p != array )
                                       fail( "completely broken" );
                                   if ( p != (char*)ulong )
                                       fail( "int2ptr . ptr2int != id" );
                                   if ( p + 1 != (char*)(ulong + 1) )
                                       fail( "int2ptr . (+1) . ptr2int != (+1)" );

                                   unsigned long v = 42;
                                   char *vp = (void*)v;
                                   if ( 42 != (unsigned long)vp )
                                       fail( "ptr2int . int2ptr != id" );
                                   if ( 43 != (unsigned long)(vp + 1) )
                                       fail( "ptr2int . (+1) . int2ptr != (+1)" );
                                   return r;
                               }
                           )|";

        ASSERT( testF( fdef ) );
    }

    TEST(simple_if)
    {
        const char* fdef = "unsigned f( int a ) { if (a > 0) { return 0; } else { return 1; } }";
        unsigned int x;
        x = testF( fdef, IntV( 0 ) );
        ASSERT_EQ( x, 1 );
        x = testF( fdef, IntV( 2 ) );
        ASSERT_EQ( x, 0 );
    }

    TEST(simple_switch)
    {
        const char* fdef = "int f( int a ) { int z; switch (a) "
                           "{ case 0: z = 0; break; case 1: case 2: z = 2; break; "
                           "case 3: case 4: return 4; default: z = 5; }; return z; }";
        unsigned int x;
        x = testF( fdef, IntV( 0 ) );
        ASSERT_EQ( x, 0 );
        x = testF( fdef, IntV( 1 ) );
        ASSERT_EQ( x, 2 );
        x = testF( fdef, IntV( 2 ) );
        ASSERT_EQ( x, 2 );
        x = testF( fdef, IntV( 3 ) );
        ASSERT_EQ( x, 4 );
        x = testF( fdef, IntV( 4 ) );
        ASSERT_EQ( x, 4 );
        x = testF( fdef, IntV( 5 ) );
        ASSERT_EQ( x, 5 );
        x = testF( fdef, IntV( -1 ) );
        ASSERT_EQ( x, 5 );
        x = testF( fdef, IntV( 6 ) );
        ASSERT_EQ( x, 5 );
    }


    TEST(recursion_01)
    {
        const char* fdef = "unsigned int f( int a ) { if (a>0) return (1 + f( a - 1 )); else return 1; }";
        unsigned int x;
        x = testF( fdef, IntV( 0 ) );
        ASSERT_EQ( x, 1 );
        x = testF( fdef, IntV( 1 ) );
        ASSERT_EQ( x, 2 );
    }

    TEST(go_to)
    {
        const char* fdef = "int f( int a ) { begin: if (a>0) { --a; goto begin; } else return -1; }";
        int x;
        x = testF( fdef, IntV( 0 ) );
        ASSERT_EQ( x, -1 );
        x = testF( fdef, IntV( 1 ) );
        ASSERT_EQ( x, -1 );
    }

    TEST(for_cycle)
    {
        const char* fdef = "int f( int a ) { int i; for (i=0; i<a; i++) i=i+2; return (i-a); }";
        int x;
        x = testF( fdef, IntV( 0 ) );
        ASSERT_EQ( x, 0 );
        x = testF( fdef, IntV( 1 ) );
        ASSERT_EQ( x, 2 );
        x = testF( fdef, IntV( -1 ) );
        ASSERT_EQ( x, 1 );
    }


    TEST(while_cycle)
    {
        const char* fdef = "int f( int a ) { a = a % 100; while ( 1 ) { if (a == 100) break; ++a; }; return a; }";
        int x;
        x = testF( fdef, IntV( 30 ) );
        ASSERT_EQ( x, 100 );
        x = testF( fdef, IntV( -30 ) );
        ASSERT_EQ( x, 100 );
    }

    TEST(bit_ops)
    {
        const char* fdef = "int f( int a ) { int b = a | 7; b = b & 4; b = b & a; return b; }";
        int x;
        x = testF( fdef, IntV( 4 ) );
        ASSERT_EQ( x, 4 );
        x = testF( fdef, IntV( 3 ) );
        ASSERT_EQ( x, 0 );
    }

    TEST(nested_loops)
    {
        const char* fdef = "int f( int a ) { int c = 0; while (a>0) { for (int b = 0; b<a; ++b) ++c; --a; } return c; }";
        int x;
        x = testF( fdef, IntV( 5 ) );
        ASSERT_EQ( x, 15 );
    }

    TEST(cmpxchg_bool)
    {
        const char *fdef = "int f( int a ) { _Atomic int v = 0; "
                            "return __c11_atomic_compare_exchange_strong( &v, &a, 42, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST );"
                            "}";
        int x;
        x = testF( fdef, IntV( 4 ) );
        ASSERT( !x );
        x = testF( fdef, IntV( 0 ) );
    }

    TEST(cmpxchg_val) {
        const char *fdef = "int f( int a ) { _Atomic int v = 1; "
                           "__c11_atomic_compare_exchange_strong( &v, &a, 42, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST );"
                           "return v; }";
        int x;
        x = testF( fdef, IntV( 4 ) );
        ASSERT_EQ( x, 1 );
        x = testF( fdef, IntV( 1 ) );
        ASSERT_EQ( x, 42 );
    }

    void testARMW( std::string op, int orig, int val, int res ) {
        std::string fdefBase = "int f( int a ) { _Atomic int v = " + std::to_string( orig ) + ";"
                             + "int r = __c11_atomic_" + op + "( &v, a, __ATOMIC_SEQ_CST );";
        std::string fdefO = fdefBase + "return r;}";
        std::string fdefN = fdefBase + "return v;}";

        ASSERT_EQ( testF( fdefO, IntV( val ) ), orig );
        ASSERT_EQ( testF( fdefN, IntV( val ) ), res );
    }

    TEST(armw_add) {
        testARMW( "fetch_add", 4, 38, 42 );
    }

    TEST(armw_sub) {
        testARMW( "fetch_sub", 4, 3, 1 );
    }

    TEST(armw_and) {
        testARMW( "fetch_and", 0xff, 0xf0, 0xf0 );
    }

    TEST(armw_or) {
        testARMW( "fetch_or", 0x0f, 0xf0, 0xff );
    }

    TEST(armw_xor) {
        testARMW( "fetch_xor", 0xf0f0, 0xf00f, 0x00ff );
    }

    TEST(armw_exchange) {
        testARMW( "exchange", 1, 2, 2 );
    }

    TEST(vastart) {
        ASSERT( testF( R"(int g( int x, ... ) {
                              __builtin_va_list va;
                              __builtin_va_start( va, x );
                              return 1;
                          }
                          int f() { return g( 0, 1 ); }
                          )" ) );
    }

    TEST(vaend) {
        ASSERT( testF( R"(int g( int x, ... ) {
                              __builtin_va_list va;
                              __builtin_va_start( va, x );
                              __builtin_va_end( va );
                              return 1;
                          }
                          int f() { return g( 0, 1 ); }
                          )" ) );
    }

    TEST(vacopy) {
        ASSERT( testF( R"(int g( int x, ... ) {
                              __builtin_va_list va, v2;
                              __builtin_va_start( va, x );
                              __builtin_va_copy( v2, va );
                              __builtin_va_end( va );
                              __builtin_va_end( v2 );
                              return 1;
                          }
                          int f() { return g( 0, 1 ); }
                          )" ) );
    }

    TEST(vaarg_1) {
        ASSERT_EQ( testF( R"(void *__lart_llvm_va_arg( __builtin_va_list va );
                             int g( int x, ... ) {
                                 __builtin_va_list va;
                                 __builtin_va_start( va, x );
                                 int v = *(int*)__lart_llvm_va_arg( va );
                                 __builtin_va_end( va );
                                 return v;
                             }
                             int f() { return g( 0, 1 ); }
                             )" ), 1 );
    }

    TEST(vaarg_2) {
        ASSERT_EQ( testF( R"(void *__lart_llvm_va_arg( __builtin_va_list va );
                             int g( int x, ... ) {
                                 __builtin_va_list va;
                                 __builtin_va_start( va, x );
                                 int v = *(int*)__lart_llvm_va_arg( va );
                                 v <<= 8;
                                 v |= *(int*)__lart_llvm_va_arg( va );
                                 v <<= 8;
                                 v |= *(int*)__lart_llvm_va_arg( va );
                                 __builtin_va_end( va );
                                 return v;
                             }
                             int f() { return g( 0, 1, 2, 3, 4 ); }
                             )" ), (1 << 16) | (2 << 8) | 3 );
    }

    TEST(vaarg_3) {
        ASSERT_EQ( testF( R"(void *__lart_llvm_va_arg( __builtin_va_list va );
                             int g( int x, ... ) {
                                 __builtin_va_list va;
                                 __builtin_va_start( va, x );
                                 int *p = *(int**)__lart_llvm_va_arg( va );
                                 __builtin_va_end( va );
                                 return *p;
                             }
                             int f() { int x = 42; return g( 0, &x ); }
                             )" ), 42 );
    }

    TEST(vaarg_4) {
        ASSERT_EQ( testF( R"(void *__lart_llvm_va_arg( __builtin_va_list va );
                             int g( int x, ... ) {
                                 __builtin_va_list va;
                                 __builtin_va_start( va, x );
                                 int v = *(int*)__lart_llvm_va_arg( va );
                                 int *p = *(int**)__lart_llvm_va_arg( va );
                                 int v2 = *(int*)__lart_llvm_va_arg( va );
                                 __builtin_va_end( va );
                                 return v + *p + v2;
                             }
                             int f() { int x = 30; return g( 0, 2, &x, 10 ); }
                             )" ), 42 );
    }

    TEST(sext)
    {
        int x = testF( "int f( int a, char b ) { return a + b; }",
                       IntV( 1 ), vm::value::Int< 8 >( -2 ) );
        ASSERT_EQ( x, -1 );
    }

    template< typename T >
    void testOverflow( int intrinsic, T a, T b, unsigned index, T out )
    {
        int x = testLLVM( [=]( auto &irb, auto *function )
            {
                auto intr = Intrinsic::getDeclaration( function->getParent(),
                                                       llvm::Intrinsic::ID( intrinsic ),
                                                       { irb.getInt32Ty() } );
                auto rs = irb.CreateCall( intr, { irb.getInt32( a ), irb.getInt32( b ) } );
                auto r = irb.CreateExtractValue( rs, { index } );
                if ( r->getType() != irb.getInt32Ty() )
                    r = irb.CreateZExt( r, irb.getInt32Ty() );
                irb.CreateRet( r );
            } );
        ASSERT_EQ( T( x ), out );
    }

    TEST(uadd_with_overflow)
    {
        uint32_t u32max = std::numeric_limits< uint32_t >::max();

        testOverflow( Intrinsic::uadd_with_overflow, u32max, 1u, 0, u32max + 1 );
        testOverflow( Intrinsic::uadd_with_overflow, u32max, 1u, 1, 1u );
        testOverflow( Intrinsic::uadd_with_overflow, u32max - 1, 1u, 1, 0u );
    }

    TEST(sadd_with_overflow)
    {
        int32_t i32max = std::numeric_limits< int32_t >::max(),
                i32min = std::numeric_limits< int32_t >::min();

        testOverflow( Intrinsic::sadd_with_overflow, i32max, 1, 0, i32max + 1 );
        testOverflow( Intrinsic::sadd_with_overflow, i32max, 1, 1, 1 );
        testOverflow( Intrinsic::sadd_with_overflow, i32max - 1, 1, 1, 0 );
        testOverflow( Intrinsic::sadd_with_overflow, i32min, -1, 0, i32min - 1 );
        testOverflow( Intrinsic::sadd_with_overflow, i32min, -1, 1, 1 );
        testOverflow( Intrinsic::sadd_with_overflow, i32min + 1, -1, 1, 0 );
    }

    TEST(usub_with_overflow)
    {
        uint32_t u32max = std::numeric_limits< uint32_t >::max();

        testOverflow( Intrinsic::usub_with_overflow, 0u, 1u, 0, u32max  );
        testOverflow( Intrinsic::usub_with_overflow, 0u, 1u, 1, 1u );
        testOverflow( Intrinsic::usub_with_overflow, 0u, 0u, 1, 0u );
        testOverflow( Intrinsic::usub_with_overflow, u32max, u32max, 1, 0u );
    }

    TEST(ssub_with_overflow)
    {
        int32_t i32max = std::numeric_limits< int32_t >::max(),
                i32min = std::numeric_limits< int32_t >::min();

        testOverflow( Intrinsic::ssub_with_overflow, i32max, -1, 0, i32min );
        testOverflow( Intrinsic::ssub_with_overflow, i32max, -1, 1, 1 );
        testOverflow( Intrinsic::ssub_with_overflow, i32max - 1, -1, 1, 0 );
        testOverflow( Intrinsic::ssub_with_overflow, i32min, 1, 0, i32max );
        testOverflow( Intrinsic::ssub_with_overflow, i32min, 1, 1, 1 );
        testOverflow( Intrinsic::ssub_with_overflow, i32min + 1, 1, 1, 0 );
        testOverflow( Intrinsic::ssub_with_overflow, 0, i32min, 0, i32min );
        testOverflow( Intrinsic::ssub_with_overflow, 0, i32min, 1, 1 );
        testOverflow( Intrinsic::ssub_with_overflow, 0, i32min + 1, 1, 0 );
    }

    TEST(umul_with_overflow)
    {
        uint32_t u32max = std::numeric_limits< uint32_t >::max();

        testOverflow( Intrinsic::umul_with_overflow, u32max, 2u, 0, u32max * 2u );
        testOverflow( Intrinsic::umul_with_overflow, u32max, 2u, 1, 1u );
        testOverflow( Intrinsic::umul_with_overflow, u32max, 2u, 1, 1u );
    }

    TEST(smul_with_overflow)
    {
        int32_t i32max = std::numeric_limits< int32_t >::max(),
                i32min = std::numeric_limits< int32_t >::min();

        testOverflow( Intrinsic::smul_with_overflow, i32max, 2, 0, i32max * 2 );
        testOverflow( Intrinsic::smul_with_overflow, i32max, 2, 1, 1 );
        testOverflow( Intrinsic::smul_with_overflow, i32min, 2, 0, i32min * 2 );
        testOverflow( Intrinsic::smul_with_overflow, i32min, 2, 1, 1 );
    }

    TEST(taint_0)
    {
        auto t = IntV( 10 );
        auto x = testP( c2prog( "int f( int a ) { return a; }" ), t );
        ASSERT_EQ( x.taints(), 0 );
    }

    TEST(taint_1)
    {
        auto t = IntV( 10 );
        t.taints( 1 );
        auto x = testP( c2prog( "int f( int a ) { return a; }" ), t );
        ASSERT_EQ( x.taints(), 1 );
    }

    TEST(taint_add)
    {
        auto t = IntV( 10 );
        t.taints( 1 );
        auto x = testP( c2prog( "int f( int a ) { return a + 5; }" ), t );
        ASSERT_EQ( x.taints(), 1 );
    }

    TEST(taint_add_vars)
    {
        auto t1 = IntV( 10 ), t2 = IntV( 4 );
        t1.taints( 1 );
        auto x = testP( c2prog( "int f( int a, int b ) { return a + b; }" ), t1, t2 );
        ASSERT_EQ( x.taints(), 1 );
    }

    TEST(taint_test_0)
    {
        auto x = testP( c2prog( "int __vm_test_taint_xi( int (*f)( char, int ), int, int ); "
                                "int g( char t, int x ) { return 7; }"
                                "int f() { return __vm_test_taint_xi( g, 3, 7 ); }" ) );
        ASSERT_EQ( x.cooked(), 3 );
    }

    TEST(taint_test_1)
    {
        auto x = testP( c2prog( "int __vm_test_taint_ii( int (*f)( char, int ), int (*g)( int ), int ); "
                                "int g( char t, int x ) { return 7; } "
                                "int h( int x ) { return 42; } "
                                "int f() { return __vm_test_taint_ii( g, h, 7 ); }" ) );
        ASSERT_EQ( x.cooked(), 42 );
    }

    TEST(taint_test_arg)
    {
        auto x = testP( c2prog( "int __vm_test_taint_ii( int (*f)( char, int ), int (*g)( int ), int ); "
                                "int g( char t, int x ) { return 7; } "
                                "int h( int x ) { return 42 + x; } "
                                "int f() { return __vm_test_taint_ii( g, h, 7 ); }" ) );
        ASSERT_EQ( x.cooked(), 49 );
    }

    TEST(taint_test_tainted)
    {
        auto t = IntV( 10 );
        t.taints( 1 );
        std::cerr << "t = " << t << std::endl;
        auto x = testP( c2prog( "int __vm_test_taint_ii( int (*f)( char, int ), int (*g)( int ), int ); "
                                "int g( char t, int x ) { return 8; } "
                                "int h( int x ) { return 42 + x; } "
                                "int f( int t ) { return __vm_test_taint_ii( g, h, t ); }" ), t );
        ASSERT_EQ( x.cooked(), 8 );
    }

    TEST(taint_test_val)
    {
        auto t = IntV( 10 );
        t.taints( 1 );
        std::cerr << "t = " << t << std::endl;
        auto x = testP( c2prog( "int __vm_test_taint_ii( int (*f)( char, int ), int (*g)( int ), int ); "
                                "int g( char t, int x ) { return 8 + x; } "
                                "int h( int x ) { return 42 + x; } "
                                "int f( int t ) { return __vm_test_taint_ii( g, h, t ); }" ), t );
        ASSERT_EQ( x.cooked(), 18 );
    }

};
#endif

}
