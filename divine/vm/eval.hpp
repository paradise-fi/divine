// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2012-2017 Petr Ročkai <code@fixp.eu>
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

#include <divine/vm/lx-code.hpp>
#include <divine/vm/program.hpp>
#include <divine/vm/value.hpp>
#include <divine/vm/context.hpp>

#include <divine/vm/divm.h>

#include <brick-data>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Intrinsics.h>
DIVINE_UNRELAX_WARNINGS

#include <algorithm>
#include <cmath>
#include <type_traits>
#include <unordered_set>

namespace divine::vm
{

long syscall_helper( int id, std::vector< long > args, std::vector< bool > argtypes );

using ::llvm::ICmpInst;
using ::llvm::FCmpInst;
using ::llvm::AtomicRMWInst;
namespace Intrinsic = ::llvm::Intrinsic;

struct NoOp { template< typename T > NoOp( T ) {} };

template< typename To > struct Convertible
{
    template< typename From >
    static constexpr decltype( To( std::declval< From >() ), true ) value( unsigned ) { return true; }
    template< typename From >
    static constexpr bool value( int ) { return false ; }

    template< typename From >
    struct Guard {
        static const bool value = Convertible< To >::value< From >( 0u );
    };
};

template< typename To > struct SignedConvertible
{
    template< typename From >
    struct Guard {
        static const bool value = std::is_arithmetic< typename To::Cooked >::value &&
                                  std::is_arithmetic< typename From::Cooked >::value &&
                                  Convertible< To >::template Guard< From >::value;
    };
};

template< typename T > struct IsIntegral
{
    static const bool value = std::is_integral< typename T::Cooked >::value;
};

template< typename T > struct IsFloat
{
    static const bool value = std::is_floating_point< typename T::Cooked >::value;
};

template< typename T > struct IntegerComparable
{
    static const bool value = std::is_integral< typename T::Cooked >::value ||
                              std::is_same< typename T::Cooked, GenericPointer >::value;
};

template< typename T > struct IsArithmetic
{
    static const bool value = std::is_arithmetic< typename T::Cooked >::value;
};

template< typename > struct Any { static const bool value = true; };

/*
 * An efficient evaluator for the LLVM instruction set. Current semantics
 * (mainly) for arithmetic are derived from C++ semantics, which may not be
 * 100% correct. There are two main APIs: dispatch() and run(), the latter of
 * which executes until the 'ret' instruction in the topmost frame is executed,
 * at which point it gives back the return value passed to that 'ret'
 * instruction. The return value type must match that of the 'Result' template
 * parameter.
 */
template < typename Context >
struct Eval
{
    Context &_context;

    using Slot = typename Program::Slot;
    using Instruction = typename Program::Instruction;

    auto &heap() { return _context.heap(); }
    auto &context() { return _context; }
    auto &program() { return _context.program(); }
    auto &types() { return *program()._types; }

    Eval( Context &c )
        : _context( c )
    {}

    using OpCode = llvm::Instruction;

    Instruction *_instruction;
    Instruction &instruction() { return *_instruction; }

    using PointerV = value::Pointer;
    using BoolV = value::Bool;
    /* TODO should be sizeof( int ) of the *bitcode*, not ours! */
    using IntV = value::Int< 8 * sizeof( int ), true >;
    using CharV = value::Int< 1, false >;
    using PtrIntV = vm::value::Int< _VM_PB_Full >;

    static_assert( Convertible< PointerV >::template Guard< IntV >::value, "" );
    static_assert( Convertible< IntV >::template Guard< PointerV >::value, "" );
    static_assert( Convertible< value::Int< 64, true > >::template Guard< PointerV >::value, "" );
    static_assert( IntegerComparable< IntV >::value, "" );

    CodePointer pc() { return context().get( _VM_CR_PC ).pointer; }

    HeapPointer frame() { return context().frame(); }
    HeapPointer globals() { return context().globals(); }
    HeapPointer constants() { return context().constants(); }

    PointerV makeobj( int size, int off = 0 )
    {
        using brick::bitlevel::mixdown;
        if ( size >= 16 * 1024 * 1024 )
        {
            fault( _VM_F_Memory ) << "only allocations smaller than 16MiB are allowed";
            return PointerV( nullPointer() );
        }
        ++ context().ref( _VM_CR_ObjIdShuffle ).integer;
        uint32_t hint = mixdown( context().get( _VM_CR_ObjIdShuffle ).integer,
                                 context().get( _VM_CR_Frame ).pointer.object() );
        return heap().make( size, hint + off );
    }

    bool freeobj( HeapPointer p )
    {
        ++ context().ref( _VM_CR_ObjIdShuffle ).integer;
        return heap().free( p );
    }

    GenericPointer s2ptr( Slot v, int off = 0 )
    {
        ASSERT_LT( v.location, Slot::Invalid );
        return context().get( v.location ).pointer + v.offset + off;
    }

    GenericPointer s2ptr( Slot v, int off, HeapPointer f )
    {
        if ( v.location == Slot::Local )
            return f + v.offset + off;
        return s2ptr( v, off );
    }

    typename Context::Heap::Loc s2loc( Slot v, int off = 0 )
    {
        auto gp = s2ptr( v, off );
        return typename Context::Heap::Loc( context().ptr2i( v.location ), gp.object(), gp.offset() );
    }

    template< typename V >
    void slot_write( Slot s, V v, int off = 0 )
    {
        context().ptr2i( s.location, heap().write( s2loc( s, off ), v ) );
    }

    void slot_copy( HeapPointer from, Slot to, int size, int offset = 0 )
    {
        auto to_l = s2loc( to, offset );
        heap().copy( heap(), heap().loc( from ), to_l, size );
        context().ptr2i( to.location, to_l.object );
    }

    template< typename V >
    void slot_read( Slot s, V &v )
    {
        heap().read( s2loc( s ), v );
    }

    template< typename V >
    void slot_read( Slot s, HeapPointer fr, V &v )
    {
        heap().read( s2ptr( s, 0, fr ), v );
    }

    Slot ptr2s( GenericPointer p )
    {
        if ( p.type() == PointerType::Global )
            return program()._globals[ p.object() ];
        else UNREACHABLE( "bad pointer in ptr2s" );
    }

    HeapPointer ptr2h( PointerV p )
    {
        auto pp = p.cooked();

        if ( pp.heap() || pp.null() )
            return pp;

        return s2ptr( ptr2s( pp ), pp.offset() );
    }

    template< typename MkF >
    bool boundcheck( MkF mkf, PointerV p, int sz, bool write, std::string dsc = "" );

    bool boundcheck( PointerV p, int sz, bool write, std::string dsc = "" )
    {
        return boundcheck( [this]( auto t ) { return fault( t ); }, p, sz, write, dsc );
    }

    bool boundcheck_nop( PointerV p, int sz, bool write )
    {
        return boundcheck( [this]( auto ) { return std::stringstream(); }, p, sz, write, "" );
    }

    int ptr2sz( PointerV p )
    {
        auto pp = p.cooked();
        if ( pp.heap() )
            return heap().size( pp );
        if ( pp.type() == PointerType::Global )
            return ptr2s( pp ).size();
        UNREACHABLE_F( "a bad pointer in ptr2sz: %s", brick::string::fmt( PointerV( p ) ).c_str() );
    }

    struct FaultStream : std::stringstream
    {
        Context *_ctx;
        Fault _fault;
        HeapPointer _frame;
        CodePointer _pc;
        bool _trace, _double;

        FaultStream( Context &c, Fault f, HeapPointer frame, CodePointer pc, bool t, bool dbl )
            : _ctx( &c ), _fault( f ), _frame( frame ), _pc( pc ), _trace( t ), _double( dbl )
        {}

        FaultStream( FaultStream &&s )
            : FaultStream( *s._ctx, s._fault, s._frame, s._pc, s._trace, s._double )
        {
            s._ctx = nullptr;
        }

        FaultStream( const FaultStream & ) = delete;

        ~FaultStream()
        {
            if ( !_ctx )
                return;
            if ( _trace )
                _ctx->trace( "FAULT: " + str() );
            if ( _double )
            {
                if ( _trace )
                    _ctx->trace( "FATAL: fault handler called recursively" );
                _ctx->doublefault();
            } else
                _ctx->fault( _fault, _frame, _pc );
        }
    };

    FaultStream fault( Fault f );
    FaultStream fault( Fault f, HeapPointer frame, CodePointer c );

    template< typename _T >
    struct V
    {
        using T = _T;
        Eval *ev;
        V( Eval *ev ) : ev( ev ) {}

        T get() { UNREACHABLE( "may only be used in decltype()" ); }

        T get( Slot s ) { T result; ev->slot_read( s, result ); return result; }
        T get( int v ) { return get( ev->instruction().value( v ) ); }
        T get( PointerV p ) { T r; ev->heap().read( ev->ptr2h( p ), r ); return r; }
        T arg( int v ) { return get( v + 1 ); }

        void set( int v, T t )
        {
            ev->slot_write( ev->instruction().value( v ), t );
        }
    };

    template< typename T > T operand( int i ) { return V< T >( this ).get( i >= 0 ? i + 1 : i ); }
    template< typename T > void result( T t ) { V< T >( this ).set( 0, t ); }
    auto operand( int i ) { return instruction().operand( i ); }
    auto result() { return instruction().result(); }

    template< typename T > auto operandCk( int i )
    {
        auto op = operand< T >( i );
        if ( !op.defined() )
            fault( _VM_F_Hypercall ) << "operand " << i << " has undefined value: " << op;
        return op;
    }

    template< typename T > auto operandCk( int idx, Instruction &insn, HeapPointer fr )
    {
        T val;
        slot_read( insn.operand( idx ), fr, val );
        if ( !val.defined() )
            fault( _VM_F_Hypercall ) << "operand " << idx << " has undefined value: "  << val;
        return val;
    }

    PointerV operandPtr( int i )
    {
        auto op = operand< PointerV >( i );
        if ( !op.defined() )
            fault( _VM_F_Hypercall ) << "pointer operand " << i << " has undefined value: " << op;
        return op;
    }

    using AtOffset = std::pair< IntV, int >;

    AtOffset compositeOffset( int t )
    {
        return std::make_pair( IntV( 0 ), t );
    }

    template< typename... Args >
    AtOffset compositeOffset( int t, IntV index, Args... indices )
    {
        auto st = types().subtype( t, index.cooked() );
        IntV offset( st.first );
        offset.defined( index.defined() );
        auto r = compositeOffset( st.second, indices... );
        return std::make_pair( r.first + offset, r.second );
    }

    IntV compositeOffsetFromInsn( int t, int current, int end )
    {
        if ( current == end )
            return IntV( 0 );

        auto index = operand< IntV >( current );
        auto r = compositeOffset( t, index );

        return r.first + compositeOffsetFromInsn( r.second, current + 1, end );
    }

    void implement_store()
    {
        auto to = operand< PointerV >( 1 );
        int sz = operand( 0 ).size();
        if ( !boundcheck( to, sz, true ) )
            return;
        auto slot = operand( 0 );
        auto mem = ptr2h( to );
        auto mem_i = heap().loc( mem ), old_i = mem_i;
        heap().copy( heap(), s2loc( slot ), mem_i, sz );
        if ( mem_i != old_i )
            context().flush_ptr2i(); /* might have affected register-held objects */
    }

    void implement_load()
    {
        auto from = operand< PointerV >( 0 );
        int sz = result().size();
        if ( !boundcheck( from, sz, false ) )
            return;
        slot_copy( ptr2h( from ), result(), sz );
    }

    template< template< typename > class Guard = Any, typename T, typename Op >
    auto op( Op _op ) -> typename std::enable_if< Guard< T >::value >::type
    {
        // std::cerr << "op called on type " << typeid( T ).name() << std::endl;
        // std::cerr << instruction() << std::endl;
        _op( V< T >( this ) );
    }

    template< template< typename > class Guard = Any, typename T >
    void op( NoOp )
    {
        // instruction().op->dump();
        UNREACHABLE_F( "invalid operation on %s", typeid( T ).name() );
    }

    template< template< typename > class Guard = Any, typename Op >
    void op( int off, Op _op )
    {
        auto v = instruction().value( off );
        return type_dispatch< Guard >( v.type, _op );
    }

    template< template< typename > class Guard = Any, typename Op >
    void type_dispatch( typename Slot::Type type, Op _op )
    {
        switch ( type )
        {
            case Slot::I1: return op< Guard, value::Int<  1 > >( _op );
            case Slot::I8: return op< Guard, value::Int<  8 > >( _op );
            case Slot::I16: return op< Guard, value::Int< 16 > >( _op );
            case Slot::I32: return op< Guard, value::Int< 32 > >( _op );
            case Slot::I64: return op< Guard, value::Int< 64 > >( _op );
            case Slot::Ptr: case Slot::PtrA: case Slot::PtrC:
                return op< Guard, PointerV >( _op );
            case Slot::F32:
                return op< Guard, value::Float< float > >( _op );
            case Slot::F64:
                return op< Guard, value::Float< double > >( _op );
            case Slot::F80:
                return op< Guard, value::Float< long double > >( _op );
            case Slot::Void:
                return;
            default:
                // instruction().op->dump();
                UNREACHABLE_F( "an unexpected dispatch type %d", type );
        }
    }

    template< template< typename > class Guard = Any, typename Op >
    void op( int off1, int off2, Op _op )
    {
        op< Any >( off1, [&]( auto v1 ) {
                return this->op< Guard< decltype( v1.get() ) >::template Guard >(
                    off2, [&]( auto v2 ) { return _op( v1, v2 ); } );
            } );
    }

    void implement_alloca()
    {
        int count = operandCk< IntV >( 0 ).cooked();
        int size = types().allocsize( instruction().subcode );

        unsigned alloc = std::max( 1, count * size );
        auto res = makeobj( alloc );
        result( res );
    }

    void implement_extractvalue()
    {
        auto off = compositeOffsetFromInsn( instruction().subcode, 1, instruction().argcount() );
        ASSERT( off.defined() );
        slot_copy( s2ptr( operand( 0 ), off.cooked() ), result(), result().size() );
    }

    void implement_insertvalue()
    {
        /* first copy the original */
        slot_copy( s2ptr( operand( 0 ) ), result(), result().size() );
        auto off = compositeOffsetFromInsn( instruction().subcode, 2, instruction().argcount() );
        ASSERT( off.defined() );
        /* write the new value over the selected field */
        slot_copy( s2ptr( operand( 1 ) ), result(), operand( 1 ).size(), off.cooked() );
    }

    void local_jump( PointerV _to )
    {
        CodePointer to( _to.cooked() );

        if ( pc().function() != to.function() )
        {
            fault( _VM_F_Control ) << "illegal cross-function jump to " << _to;
            return;
        }
        jump( to );
    }

    bool long_jump( CodePointer to )
    {
        if ( jump( to ) )
            if ( to.function() &&
                 program().instruction( to ).opcode != lx::OpBB &&
                 to.instruction() + 1 == program().function( to ).instructions.size() )
            {
                fault( _VM_F_Control, HeapPointer(), CodePointer() )
                    << "illegal long jump to function end";
                return false;
            }
        return true;
    }


    bool jump( CodePointer to )
    {
        if ( program().functions.size() <= to.function() )
        {
            fault( _VM_F_Control ) << "illegal jump to a non-existent function: " << to;
            return false;
        }

        if ( to.function() && program().function( to ).instructions.size() <= to.instruction() )
        {
            fault( _VM_F_Control ) << "illegal jump beyond function end: " << to;
            return false;
        }

        switchBB( to );
        return true;
    }


    HeapPointer _final_frame;

    template< typename R >
    R retval()
    {
        R res;

        ASSERT( context().ref( _VM_CR_Flags ).integer & _VM_CF_KernelMode );
        ASSERT( context().get( _VM_CR_Frame ).pointer.null() );
        ASSERT( !_final_frame.null() );
        ASSERT_EQ( instruction().opcode, OpCode::Ret );
        context().set( _VM_CR_Frame, _final_frame );

        if ( instruction().argcount() )
            res = operand< R >( 0 );

        freeobj( _final_frame );
        context().set( _VM_CR_Frame, nullPointer() );
        _final_frame = HeapPointer();

        return res;
    }

    void implement_ret();
    void implement_br();

    void implement_indirectBr()
    {
        local_jump( operandCk< PointerV >( 0 ) );
    }

    template< typename F >
    void each_phi( CodePointer first, F f );
    void switchBB( CodePointer target );
    void update_shuffle();

    template< typename Y >
    void collect_allocas( CodePointer pc, Y yield );
    void implement_stacksave();
    void implement_stackrestore();

    template< typename T >
    static constexpr auto _maxbound( T ) { return std::numeric_limits< T >::max(); };
    template< typename T >
    static constexpr auto _minbound( T ) { return std::numeric_limits< T >::min(); };

    template< typename V >
    void check( V v ) { if ( !v.defined() ) fault( _VM_F_Hypercall ); }

    void implement_intrinsic( int id );

    void implement_ctl_set();
    void implement_ctl_set_frame();
    void implement_ctl_get();
    void implement_ctl_flag();

    void implement_test_loop();
    void implement_test_crit();
    void implement_test_taint();

    void implement_hypercall_syscall();
    void implement_hypercall();
    void implement_call( bool invoke );

    void implement_dbg_call()
    {
        if ( context().enter_debug() )
            implement_call( false );
    }

    void run();
    bool run_seq( bool continued );
    void dispatch(); /* evaluate a single instruction */

    bool assert_flag( uint64_t flag, std::string str )
    {
        if ( context().flags_all( flag ) )
            return true;

        fault( _VM_F_Access ) << str;
        return false;
    }

    void advance()
    {
        ASSERT_EQ( CodePointer( context().get( _VM_CR_PC ).pointer ), pc() );
        context().count_instruction();
        context().set( _VM_CR_PC, program().nextpc( pc() + 1 ) );
        _instruction = &program().instruction( pc() );
    }
};

}

#include <divine/vm/mem-heap.hpp>

namespace divine::t_vm
{

using vm::CodePointer;
namespace Intrinsic = ::llvm::Intrinsic;

template< typename Prog >
struct TContext : vm::Context< Prog, vm::mem::SmallHeap >
{
    using Super = vm::Context< Prog, vm::mem::SmallHeap >;
    vm::Fault _fault;

    void fault( vm::Fault f, vm::HeapPointer, CodePointer )
    {
        _fault = f;
        this->set( _VM_CR_Frame, vm::nullPointer() );
    }

    using Super::trace;
    void trace( std::string s ) { std::cerr << "T: " << s << std::endl; }
    void trace( vm::TraceDebugPersist ) { UNREACHABLE( "debug persist not allowed in unit tests" ); }

    TContext( Prog &p ) : vm::Context< Prog, vm::mem::SmallHeap >( p ), _fault( _VM_F_NoFault ) {}
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
            return testF( "int array[8] = { 0, 1, 2, 3 }; int f() { return array[ " + std::to_string( i ) + " ]; }" );
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
            return testF( "int array[8] = { 0 }; int f() { for ( int i = 0; i < 4; ++i ) array[i] = i; return array[ " + std::to_string( i ) + " ]; }" );
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
            return testF( "int f() { int array[4]; for ( int i = 0; i < 4; ++i ) array[i] = i; return array[ " + std::to_string( i ) + " ]; }" );
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
        const char* fdef = "int f( int a ) { int z; switch (a) { case 0: z = 0; break; case 1: case 2: z = 2; break; case 3: case 4: return 4; default: z = 5; }; return z; }";
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
