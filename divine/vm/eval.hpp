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
template < typename Context_ >
struct Eval
{
    using Context = Context_;
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

    CodePointer pc() { return context().pc(); }

    HeapPointer frame() { return context().frame(); }
    HeapPointer globals() { return context().globals(); }
    HeapPointer constants() { return context().constants(); }

    static uint32_t squash( uint32_t v, uint32_t l, uint32_t h )
    {
        return l + ( uint64_t( v ) * ( h - l ) ) / UINT32_MAX;
    }

    PointerV makeobj( int64_t size, PointerType t = PointerType::Heap )
    {
        using brick::bitlevel::mixdown;
        if ( size >= 16 * 1024 * 1024 )
        {
            fault( _VM_F_Memory ) << "only allocations smaller than 16MiB are allowed";
            return PointerV( nullPointer() );
        }
        ++ context().objid_shuffle();
        uint32_t hint = mixdown( context().objid_shuffle(), context().frame().object() );
        const auto &limits = __vm_pointer_limits[ int( t ) ];
        hint = squash( hint, limits.low, limits.high );
        auto p = heap().make( size, hint );
        ASSERT_EQ( p.cooked().type(), t );
        return p;
    }

    bool freeobj( HeapPointer p )
    {
        ++ context().objid_shuffle();
        return heap().free( p );
    }

    GenericPointer s2ptr( Slot v, int off = 0 )
    {
        ASSERT_LT( v.location, Slot::Invalid );
        return context().get_ptr( _VM_ControlRegister( v.location ) ) + v.offset + off;
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

    template< typename V > void slot_read( Slot s, V &v );
    template< typename V > void slot_read( Slot s, HeapPointer fr, V &v );

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
                _ctx->trace( TraceFault{ str() } );
            if ( _double )
            {
                if ( _trace )
                    _ctx->trace( "DOUBLE FAULT: " + _ctx->fault_str() );
                _ctx->doublefault();
            } else
                _ctx->fault( _fault, _frame, _pc );
        }
    };

    FaultStream fault( Fault f );
    FaultStream fault( Fault f, HeapPointer frame, CodePointer c );

    template< typename T > T operand( int i );
    template< typename T > void result( T t );
    auto operand( int i ) { return instruction().operand( i ); }
    auto result() { return instruction().result(); }

    template< typename T > auto operandCk( int i );
    template< typename T > auto operandCk( int idx, Instruction &insn, HeapPointer fr );

    PointerV operandPtr( int i )
    {
        auto op = operand< PointerV >( i );
        if ( !op.defined() )
            fault( _VM_F_Hypercall ) << "pointer operand " << i << " has undefined value: " << op;
        return op;
    }

    IntV gep( int type, int idx, int end ); // getelementptr

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

    template< template< typename > class Guard = Any, typename Op >
    void type_dispatch( typename Slot::Type type, Op _op, Slot s = Slot() );

private:
    template< template< typename > class Guard = Any, typename T, typename Op, typename... Args >
    auto op( Op _op, Args... args ) -> typename std::enable_if< Guard< T >::value >::type;

    template< template< typename > class Guard = Any, typename T, typename... Args >
    void op( NoOp, Args... args );

    template< template< typename > class Guard = Any, typename Op >
    void op( int off, Op _op );

    template< template< typename > class Guard = Any, typename Op >
    void op( int off1, int off2, Op _op );

public:
    void implement_alloca()
    {
        int count = operandCk< IntV >( 0 ).cooked();
        int64_t size = types().allocsize( instruction().subcode );

        int64_t alloc = std::max( int64_t( 1 ), count * size );
        auto res = makeobj( alloc, PointerType::Alloca );
        result( res );
    }

    void implement_extractvalue();
    void implement_insertvalue();

    void local_jump( PointerV _to )
    {
        if ( _to.cooked().type() == PointerType::Code && pc().object() != _to.cooked().object() )
        {
            fault( _VM_F_Control ) << "illegal cross-function jump to " << _to;
            return;
        }

        jump( _to );
    }

    bool long_jump( PointerV _to )
    {
        if ( jump( _to ) )
        {
            CodePointer to( _to.cooked() );
            if ( to.function() &&
                 program().instruction( to ).opcode != lx::OpBB &&
                 to.instruction() + 1 == program().function( to ).instructions.size() )
            {
                fault( _VM_F_Control, HeapPointer(), CodePointer() )
                    << "illegal long jump to function end";
                return false;
            }
        }
        return true;
    }


    bool jump( PointerV _to )
    {
        if ( _to.cooked().type() != PointerType::Code )
        {
            fault( _VM_F_Control ) << "illegal jump to a non-code pointer: " << _to;
            return false;
        }

        CodePointer to( _to.cooked() );

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

        ASSERT( context().flags() & _VM_CF_KernelMode );
        ASSERT( context().frame().null() );
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
    void implement_indirectBr();

    template< typename F >
    void each_phi( CodePointer first, F f );
    void switchBB( CodePointer target );
    void update_shuffle();

    template< typename Y >
    void collect_allocas( CodePointer pc, Y yield );
    template< typename Y >
    void collect_frame_locals( CodePointer pc, Y yield );
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

    void implement_peek();
    void implement_poke();

    void implement_hypercall_syscall();
    void implement_hypercall_clone();
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
        ASSERT_EQ( CodePointer( context().pc() ), pc() );
        context().count_instruction();
        context().set( _VM_CR_PC, program().nextpc( pc() + 1 ) );
        refresh();
    }

    void refresh()
    {
        _instruction = &program().instruction( pc() );
    }
};

}
