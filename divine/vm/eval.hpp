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

namespace {

long syscall_helper( int id, std::vector< long > args, std::vector< bool > argtypes )
{
    using A = std::vector< bool >;

    if ( argtypes == A{} )
        return syscall( id );
    else if ( argtypes == A{0} )
        return syscall( id, int( args[0] ) );
    else if ( argtypes == A{1} )
        return syscall( id, args[0] );
    else if ( argtypes == A{0, 0} )
        return syscall( id, int( args[0] ), int( args[1] ) );
    else if ( argtypes == A{0, 1} )
        return syscall( id, int( args[0] ), args[1] );
    else if ( argtypes == A{1, 0} )
        return syscall( id, args[0] , int( args[1] ));
    else if ( argtypes == A{1, 1} )
        return syscall( id, args[0], args[1] );
    else if ( argtypes == A{0, 0, 0} )
        return syscall( id, int( args[0] ), int( args[1] ), int( args[2] ) );
    else if ( argtypes == A{0, 0, 1} )
        return syscall( id, int( args[0] ), int( args[1] ), args[2] );
    else if ( argtypes == A{0, 1, 0} )
        return syscall( id, int( args[0] ), args[1], int( args[2] ) );
    else if ( argtypes == A{0, 1, 1} )
        return syscall( id, int( args[0] ), args[1], args[2] );
    else if ( argtypes == A{1, 0, 0} )
        return syscall( id, args[0], int( args[1] ), int( args[2] ) );
    else if ( argtypes == A{1, 0, 1} )
        return syscall( id, args[0], int( args[1] ), args[2] );
    else if ( argtypes == A{1, 1, 0} )
        return syscall( id, args[0], args[1], int( args[2] ) );
    else if ( argtypes == A{1, 1, 1} )
        return syscall( id, args[0], args[1], args[2] );
    else if ( argtypes == A{0, 1, 1, 0, 1} )
        return syscall( id, int(args[0]), args[1], args[2], int(args[3]), args[4] );
    else if ( argtypes == A{0, 1, 1, 0, 1, 1} )
        return syscall( id, int(args[0]), args[1], args[2], int(args[3]), args[4], args[5] );
    else if ( argtypes == A{0, 1, 1, 0, 1, 0} )
        return syscall( id, int(args[0]), args[1], args[2], int(args[3]), args[4], int(args[5]) );
    else {
        std::cerr << brick::string::fmt(argtypes) << std::endl;
        NOT_IMPLEMENTED();
    }
}

}

namespace llvm {
template<typename T> class generic_gep_type_iterator;
typedef generic_gep_type_iterator<User::const_op_iterator> gep_type_iterator;
}

namespace divine::vm
{

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
template < typename Context, typename Result >
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
    Result _result;

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

    template< typename V >
    void slot_write( Slot s, V v, int off = 0 )
    {
        context().ptr2i( s.location,
                         heap().write( s2ptr( s, off ), v, context().ptr2i( s.location ) ) );
    }

    void slot_copy( HeapPointer from, Slot to, int size, int offset = 0 )
    {
        auto to_i = context().ptr2i( to.location );
        heap().copy( heap(), from, heap().ptr2i( from ), s2ptr( to, offset ), to_i, size );
        context().ptr2i( to.location, to_i );
    }

    template< typename V >
    void slot_read( Slot s, V &v )
    {
        heap().read( s2ptr( s ), v, context().ptr2i( s.location ) );
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
    bool boundcheck( MkF mkf, PointerV p, int sz, bool write, std::string dsc = "" )
    {
        auto pp = p.cooked();
        int width = 0;

        if ( !p.defined() )
        {
            mkf( _VM_F_Memory ) << "undefined pointer dereference: " << p << dsc;
            return false;
        }

        if ( pp.null() )
        {
            mkf( _VM_F_Memory ) << "null pointer dereference: " << p << dsc;
            return false;
        }

        if ( pp.type() == PointerType::Code )
        {
            mkf( _VM_F_Memory ) << "attempted to dereference a code pointer " << p << dsc;
            return false;
        }

        if ( !p.pointer() )
        {
            mkf( _VM_F_Memory ) << "attempted to dereference a broken pointer " << p << dsc;
            return false;
        }

        if ( pp.heap() )
        {
            HeapPointer hp = pp;
            if ( hp.null() || !heap().valid( hp ) )
            {
                mkf( _VM_F_Memory ) << "invalid heap pointer dereference " << p << dsc;
                return false;
            }
            width = heap().size( hp );
        }
        else if ( pp.type() == PointerType::Global )
        {
            if ( write && ptr2s( pp ).location == Slot::Const )
            {
                mkf( _VM_F_Memory ) << "attempted write to a constant location " << p << dsc;
                return false;
            }

            if ( pp.object() >= program()._globals.size() )
            {
                mkf( _VM_F_Memory ) << "pointer object out of bounds in " << p << dsc;
                return false;
            }

            width = ptr2s( pp ).size();
        }

        if ( int( pp.offset() ) + sz > width )
        {
            mkf( _VM_F_Memory ) << "access of size " << sz << " at " << p
                                << " is " << pp.offset() + sz - width
                                << " bytes out of bounds";
            return false;
        }
        return true;
    }

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

    FaultStream fault( Fault f )
    {
        if ( frame().null() || !heap().valid( frame() ) )
            return fault( f, nullPointer( PointerType::Heap ),
                             nullPointer( PointerType::Code ) );
        else
            return fault( f, frame(), pc() );
    }

    FaultStream fault( Fault f, HeapPointer frame, CodePointer c )
    {
        PointerV fr( frame );
        PointerV fpc;
        while ( !fr.cooked().null() && heap().valid( fr.cooked() ) )
        {
            heap().read_shift( fr, fpc );
            if ( fpc.cooked().object() == context().get( _VM_CR_FaultHandler ).pointer.object() )
                return FaultStream( context(), f, frame, c, true, true );
            heap().read( fr.cooked(), fr );
        }

        context().sync_pc();
        FaultStream fs( context(), f, frame, c, true, false );
        return fs;
    }

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
        auto mem_i = heap().ptr2i( mem ), old_i = mem_i;
        heap().copy( heap(), s2ptr( slot ), context().ptr2i( slot.location ), mem, mem_i, sz );
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

    void jumpTo( PointerV _to )
    {
        CodePointer to( _to.cooked() );
        if ( pc().function() != to.function() ) {
            fault( _VM_F_Control ) << "illegal cross-function jump to " << _to;
            return;
        }
        switchBB( to );
    }

    void implement_ret()
    {
        PointerV fr( frame() );
        heap().skip( fr, sizeof( typename PointerV::Raw ) );
        PointerV parent, br;
        heap().read( fr.cooked(), parent );

        collect_allocas( [&]( auto ptr )
                         {
                             freeobj( ptr.cooked() );
                         } );

        if ( parent.cooked().null() )
        {
            bool usermode = false;
            if ( context().ref( _VM_CR_Flags ).integer & _VM_CF_KernelMode )
            {
                if ( instruction().argcount() )
                    _result = operand< Result >( 0 );
            }
            else
            {
                context().mask( false );
                context().set_interrupted( true );
                usermode = true;
            }
            context().set( _VM_CR_Frame, nullPointer() );
            freeobj( fr.cooked() );
            if ( usermode )
                context().check_interrupt( *this );
            return;
        }

        PointerV caller_pc;
        heap().read( parent.cooked(), caller_pc );
        const auto &caller = program().instruction( caller_pc.cooked() );
        if ( instruction().argcount() ) /* return value */
        {
            if ( !caller.has_result() ) {
                fault( _VM_F_Control, parent.cooked(), caller_pc.cooked() )
                    << "Function which was called as void returned a value";
                return;
            } else if ( caller.result().size() != operand( 0 ).size() ) {
                fault( _VM_F_Control, parent.cooked(), caller_pc.cooked() )
                    << "Returned value is bigger then expected by caller";
                return;
            } else if ( !heap().copy( s2ptr( operand( 0 ) ),
                                    s2ptr( caller.result(), 0, parent.cooked() ),
                                    caller.result().size() ) )
            {
                fault( _VM_F_Memory, parent.cooked(), caller_pc.cooked() )
                    << "Cound not return value";
                return;
            }
        } else if ( caller.result().size() > 0 ) {
            fault( _VM_F_Control, parent.cooked(), caller_pc.cooked() )
                << "Function called as non-void did not return a value";
            return;
        }

        auto retpc = pc();
        context().set( _VM_CR_Frame, parent.cooked() );
        context().set( _VM_CR_PC, caller_pc.cooked() );

        if ( caller.opcode == OpCode::Invoke )
        {
            auto rv = s2ptr( caller.operand( -2 ), 0, parent.cooked() );
            heap().read( rv, br );
            jumpTo( br );
        }
        freeobj( fr.cooked() );
        context().left( retpc );
    }

    void implement_br()
    {
        if ( instruction().argcount() == 1 )
            jumpTo( operandCk< PointerV >( 0 ) );
        else
        {
            auto cond = operand< BoolV >( 0 );
            auto target = operandCk< PointerV >( cond.cooked() ? 2 : 1 );
            if ( !cond.defined() )
                fault( _VM_F_Control, frame(), target.cooked() )
                    << " conditional jump depends on an undefined value";
            else
                jumpTo( target );
        }
    }

    void implement_indirectBr()
    {
        jumpTo( operandCk< PointerV >( 0 ) );
    }

    template< typename F >
    void each_phi( CodePointer first, F f )
    {
        auto pc = first;
        while ( true )
        {
            auto &i = program().instruction( pc );
            if ( i.opcode != OpCode::PHI )
                return;
            f( i );
            pc.instruction( pc.instruction() + 1 );
        }
    }

    /*
     * Two-phase PHI handler. We do this because all of the PHI nodes must be
     * executed atomically, reading their inputs before any of the results are
     * updated.  Not doing this can cause problems if the PHI nodes depend on
     * other PHI nodes for their inputs.  If the input PHI node is updated
     * before it is read, incorrect results can happen.
     */

    void switchBB( CodePointer target )
    {
        auto origin = pc();
        context().set( _VM_CR_PC, target );

        target.instruction( target.instruction() + 1 );
        auto &i0 = program().instruction( target );
        if ( i0.opcode != OpCode::PHI )
            return;

        int size = 0, count = 0, incoming = i0.argcount() / 2, idx = -1;
        ASSERT_LEQ( 0, incoming );
        for ( int i = 0; i < incoming; ++ i )
        {
            PointerV ptr;
            slot_read( i0.operand( incoming + i ), ptr );
            CodePointer terminator( ptr.cooked() );
            ASSERT_EQ( terminator.function(), target.function() );
            if ( terminator == origin )
                idx = i;
        }
        ASSERT_LEQ( 0, idx );

        each_phi( target, [&]( auto &i )
                  {
                      size += i.result().size();
                      ++ count;
                  } );
        auto tmp = makeobj( size ), tgt = tmp;
        each_phi( target, [&]( auto &i )
                  {
                      heap().copy( s2ptr( i.operand( idx ) ), tgt.cooked(), i.result().size() );
                      heap().skip( tgt, i.result().size() );
                  } );
        tgt = tmp;
        each_phi( target, [&]( auto &i )
                  {
                      slot_copy( tgt.cooked(), i.result(), i.result().size() );
                      heap().skip( tgt, i.result().size() );
                  } );

        freeobj( tmp.cooked() );
        target.instruction( target.instruction() + count - 1 );
        context().set( _VM_CR_PC, target );
    }

    template< typename Y >
    void collect_allocas( Y yield )
    {
        auto &f = program().functions[ pc().function() ];
        for ( auto &i : f.instructions )
            if ( i.opcode == OpCode::Alloca )
            {
                PointerV ptr;
                slot_read( i.result(), ptr );
                if ( heap().valid( ptr.cooked() ) )
                    yield( ptr );
            }
    }

    void implement_stacksave()
    {
        std::vector< PointerV > ptrs;
        collect_allocas( [&]( PointerV p ) { ptrs.push_back ( p ); } );
        auto r = makeobj( sizeof( IntV::Raw ) + ptrs.size() * PointerBytes );
        auto p = r;
        heap().write_shift( p, IntV( ptrs.size() ) );
        for ( auto ptr : ptrs )
            heap().write_shift( p, ptr );
        result( r );
    }

    void implement_stackrestore()
    {
        auto r = operand< PointerV >( 0 );

        if ( !boundcheck( r, sizeof( int ), false ) )
            return;

        IntV count;
        heap().read_shift( r, count );
        if ( !count.defined() )
            fault( _VM_F_Hypercall ) << " stackrestore with undefined count";
        auto s = r;

        collect_allocas( [&]( auto ptr )
        {
            bool retain = false;
            r = s;
            for ( int i = 0; i < count.cooked(); ++i )
            {
                PointerV p;
                heap().read_shift( r, p );
                auto eq = ptr == p;
                if ( !eq.defined() )
                {
                    fault( _VM_F_Hypercall ) << " undefined pointer at index "
                                             << i << " in stackrestore";
                    break;
                }
                if ( eq.cooked() )
                {
                    retain = true;
                    break;
                }
            }
            if ( !retain )
                freeobj( ptr.cooked() );
        } );
    }

    template< typename T >
    static constexpr auto _maxbound( T ) { return std::numeric_limits< T >::max(); };
    template< typename T >
    static constexpr auto _minbound( T ) { return std::numeric_limits< T >::min(); };

    void implement_intrinsic( int id ) {

        auto _arith_with_overflow = [this]( auto wrap, auto impl, auto check ) -> void {
            return op< IsIntegral >( 1, [&]( auto v ) {
                    auto a = wrap( v.get( 1 ) ),
                         b = wrap( v.get( 2 ) );
                    auto r = impl( a, b );
                    BoolV over( check( a.cooked(), b.cooked() ) );
                    if ( !r.defined() )
                        over.defined( false );

                    slot_write( result(), r, 0 );
                    slot_write( result(), over, sizeof( typename decltype( a )::Raw ) );
                } );
        };

        auto _make_signed = []( auto x ) { return x.make_signed(); };
        auto _id = []( auto x ) { return x; };

        switch ( id ) {
            case Intrinsic::vastart:
            {
                // writes a pointer to a monolithic block of memory that
                // contains all the varargs, successively assigned higher
                // addresses (going from left to right in the argument list) to
                // the argument of the intrinsic
                const auto &f = program().functions[ pc().function() ];
                if ( !f.vararg ) {
                    fault( _VM_F_Hypercall ) << "va_start called in a non-variadic function";
                    return;
                }
                auto vaptr_loc = s2ptr( f.instructions[ f.argcount ].result() );
                auto vaList = operand< PointerV >( 0 );
                if ( !boundcheck( vaList, operand( 0 ).size(), true ) )
                    return;
                heap().copy( vaptr_loc, ptr2h( vaList ), operand( 0 ).size() );
                return;
            }
            case Intrinsic::vaend: return;
            case Intrinsic::vacopy:
            {
                auto from = operand< PointerV >( 1 );
                auto to = operand< PointerV >( 0 );
                if ( !boundcheck( from, operand( 0 ).size(), false ) ||
                     !boundcheck( to, operand( 0 ).size(), true ) )
                    return;
                heap().copy( ptr2h( from ), ptr2h( to ), operand( 0 ).size() );
                return;
            }
            case Intrinsic::trap:
                fault( _VM_F_Control ) << "llvm.trap executed";
                return;
            case Intrinsic::umul_with_overflow:
                return _arith_with_overflow( _id, []( auto a, auto b ) { return a * b; },
                                             []( auto a, auto b ) { return a > _maxbound( a ) / b; } );
            case Intrinsic::uadd_with_overflow:
                return _arith_with_overflow( _id, []( auto a, auto b ) { return a + b; },
                                             []( auto a, auto b ) { return a > _maxbound( a ) - b; } );
            case Intrinsic::usub_with_overflow:
                return _arith_with_overflow( _id, []( auto a, auto b ) { return a - b; },
                                             []( auto a, auto b ) { return b > a; } );

            case Intrinsic::smul_with_overflow:
                return _arith_with_overflow( _make_signed, []( auto a, auto b ) { return a * b; },
                                             []( auto a, auto b ) { return (a > _maxbound( a ) / b)
                                                                        || (a < _minbound( a ) / b)
                                                                        || ((a == -1) && (b == _minbound( a )))
                                                                        || ((b == -1) && (a == _minbound( a ))); } );
            case Intrinsic::sadd_with_overflow:
                return _arith_with_overflow( _make_signed, []( auto a, auto b ) { return a + b; },
                                             []( auto a, auto b ) { return b > 0
                                                    ? a > _maxbound( a ) - b
                                                    : a < _minbound( a ) - b; } );
            case Intrinsic::ssub_with_overflow:
                return _arith_with_overflow( _make_signed, []( auto a, auto b ) { return a - b; },
                                             []( auto a, auto b ) { return b < 0
                                                    ? a > _maxbound( a ) + b
                                                    : a < _minbound( a ) + b; } );

            case Intrinsic::stacksave:
                return implement_stacksave();
            case Intrinsic::stackrestore:
                return implement_stackrestore();
            case Intrinsic::dbg_value: case Intrinsic::dbg_declare:
                return; /* do nothing */
            default:
                /* We lowered everything else in buildInfo. */
                // instruction().op->dump();
                UNREACHABLE_F( "unexpected intrinsic %d", id );
        }
    }

    template< typename V >
    void check( V v ) { if ( !v.defined() ) fault( _VM_F_Hypercall ); }

    void implement_hypercall_control()
    {
        int idx = 0;

        auto o_frame = frame();
        auto &o_inst = instruction();

        while ( idx < instruction().argcount() - 1 )
        {
            int action = operandCk< IntV >( idx++, o_inst, o_frame ).cooked();
            auto reg = _VM_ControlRegister( operandCk< IntV >( idx++, o_inst, o_frame ).cooked() );
            if ( action == _VM_CA_Set && reg == _VM_CR_Flags )
            {
                auto val = operandCk< PointerV >( idx++, o_inst, o_frame ).cooked();
                auto orig = context().get( reg ).integer;
                if ( ( val.raw() & _VM_CF_KernelMode ) && !( orig & _VM_CF_KernelMode ) )
                    fault( _VM_F_Hypercall ) << "cannot set kernel mode outside of kernel";
                else
                    context().set( reg, val );
            }
            else if ( action == _VM_CA_Set && reg == _VM_CR_PC )
            {
                auto ptr = operandCk< PointerV >( idx++, o_inst, o_frame ).cooked();
                if ( ptr.type() == PointerType::Code )
                    switchBB( ptr );
                else
                {
                    fault( _VM_F_Hypercall ) << "invalid pointer type when setting _VM_CR_PC: " << ptr;
                    return;
                }
            }
            else if ( action == _VM_CA_Set )
            {
                context().sync_pc();
                auto ptr = operandCk< PointerV >( idx++, o_inst, o_frame );
                if ( reg == _VM_CR_Frame && !ptr.cooked().null() &&
                     !boundcheck_nop( ptr, 2 * PointerBytes, true ) )
                    fault( _VM_F_Hypercall ) << "invalid target frame in __vm_control";
                else
                    context().set( reg, ptr.cooked() );
            }
            else if ( action == _VM_CA_Get && reg == _VM_CR_Flags )
            {
                if ( o_frame == frame() )
                    result( PtrIntV( context().get( reg ).integer ) );
                else
                    fault( _VM_F_Hypercall ) << "invalid _VM_CA_Get after frame change";
            }
            else if ( action == _VM_CA_Get )
            {
                if ( o_frame == frame() )
                    result( PointerV( context().get( reg ).pointer ) );
                else
                    fault( _VM_F_Hypercall ) << "invalid _VM_CA_Get after frame change";
            }
            else if ( action == _VM_CA_Bit && reg == _VM_CR_Flags )
            {
                auto &reg_r = context().ref( reg ).integer;
                auto mask = operandCk< PtrIntV >( idx++, o_inst, o_frame ).cooked();
                auto val = operandCk< PtrIntV >( idx++, o_inst, o_frame ).cooked();
                if ( mask & _VM_CF_DebugMode )
                    fault( _VM_F_Hypercall ) << "cannot change debug mode";
                else if ( !( reg_r & _VM_CF_KernelMode ) && ( val & mask & _VM_CF_KernelMode ) )
                    fault( _VM_F_Hypercall ) << "cannot set kernel mode outside of kernel";
                else
                {
                    reg_r &= ~mask;
                    reg_r |= val;
                }
            }
            else if ( action == _VM_CA_DestroyFrame )
                heap().free( o_frame );
            else
            {
                fault( _VM_F_Hypercall ) << "invalid __vm_control sequence at index " << idx;
                return;
            }
            if ( action == _VM_CA_Set && reg == _VM_CR_Frame )
            {
                PointerV ptr;
                if ( !frame().null() )
                {
                    using brick::bitlevel::mixdown;
                    heap().read( frame(), ptr, context().ptr2i( _VM_CR_Frame ) );
                    context().set( _VM_CR_PC, ptr.cooked() );
                    context().ref( _VM_CR_ObjIdShuffle ).integer = mixdown(
                            heap().objhash( context().ptr2i( _VM_CR_Frame ) ),
                            context().get( _VM_CR_Frame ).pointer.object() );
                }
            }
        }
    }

    void implement_hypercall_syscall()
    {
        int idx = 0;

        std::vector< long > args;
        std::vector< bool > argt;
        std::vector< char * > bufs;

        std::vector< int >  actions;

        auto type = [&]( int action ) { return action & ~( _VM_SC_In | _VM_SC_Out ); };
        auto in   = [&]( int action ) { return action & _VM_SC_In; };
        auto out  = [&]( int action ) { return action & _VM_SC_Out; };
        auto next = [&]( int action ) { idx += type( action ) == _VM_SC_Mem ? 2 : 1; };

        auto prepare = [&]( int action )
        {
            if ( type( action ) < 0 || type( action ) > 2 || ( !in( action ) && !out( action ) ) )
            {
                fault( _VM_F_Hypercall ) << "illegal syscall parameter no " << idx << ": " << action;
                return false;
            }

            if ( in( action ) || type( action ) == _VM_SC_Mem )
                argt.push_back( type( action ) != _VM_SC_Int32 );

            if ( type( action ) == _VM_SC_Int32 && in( action ) )
                args.push_back( operandCk< IntV >( idx ).cooked() );
            else if ( type( action ) == _VM_SC_Int64 && in( action ) )
                args.push_back( operandCk< PtrIntV >( idx ).cooked() );
            else if ( type( action ) == _VM_SC_Mem )
            {
                int size = operandCk< IntV >( idx ).cooked();
                bufs.push_back( size ? new char[ size ] : nullptr );
                args.push_back( long( bufs.back() ) );
                auto ptr = operand< PointerV >( idx + 1 );
                CharV ch;
                if ( !ptr.cooked().null() && !boundcheck( ptr, size, out( action ) ) )
                    return false;
                ptr = PointerV( ptr2h( ptr ) );
                if ( in( action ) )
                    for ( int i = 0; i < size; ++i )
                    {
                        heap().read_shift( ptr, ch );
                        if ( !ch.defined() )
                        {
                            fault( _VM_F_Hypercall ) << "uninitialised byte in __vm_syscall";
                            return false;
                        }
                        bufs.back()[ i ] = ch.cooked();
                    }
            }
            else if ( out( action ) )
            {
                auto ptr = operand< PointerV >( idx );
                if ( !ptr.cooked().null() &&
                     !boundcheck( ptr, type( action ) == _VM_SC_Int32 ? 4 : 8, true ) )
                    return false;
            }
            return true;
        };

        int buf_idx = 0;

        auto process = [&]( int action, long val )
        {
            if ( type( action ) == _VM_SC_Mem )
            {
                if ( out( action ) )
                {
                    int size = operand< IntV >( idx ).cooked();
                    auto ptr = PointerV( ptr2h( operand< PointerV >( idx + 1 ) ) );
                    for ( int i = 0; i < size; ++i )
                        heap().write_shift( ptr, CharV( bufs[ buf_idx ][ i ] ) );
                }
                ++ buf_idx;
            }
            else if ( out( action ) )
            {
                auto ptr = operand< PointerV >( idx );
                if (!ptr.cooked().null()) {
                    if ( type( action ) == _VM_SC_Int32 )
                        heap().write( ptr2h(ptr), IntV( val ) );
                    else
                        heap().write( ptr2h(ptr), PtrIntV( val ) );
                }
            }
        };

        if ( instruction().argcount() < 2 )
        {
            fault( _VM_F_Hypercall ) << "at least 2 arguments are required for __vm_syscall";
            return;
        }

        IntV id = operandCk< IntV >( idx++ );

        while ( idx < instruction().argcount() - 1 )
        {
            actions.push_back( operandCk< IntV >( idx++ ).cooked() );
            if ( !prepare( actions.back() ) )
                return;
            next( actions.back() );
        }

        auto rv = syscall_helper( id.cooked(), args, argt );
        result( IntV( errno ) );

        idx = 2;
        process( actions[ 0 ], rv );
        next( actions[ 0 ] );

        for ( int i = 1; i < int( actions.size() ); ++i )
        {
            idx ++; // skip action
            process( actions[ i ], args[ i - 1 ] );
            next( actions[ i ] );
        }

        for ( auto buf : bufs )
            delete[] buf;
    }

    void implement_hypercall()
    {
        switch( instruction().subcode )
        {
            case lx::HypercallChoose:
            {
                int options = operandCk< IntV >( 0 ).cooked();
                std::vector< int > p;
                for ( int i = 1; i < int( instruction().argcount() ) - 1; ++i )
                    p.push_back( operandCk< IntV >( i ).cooked() );
                if ( !p.empty() && int( p.size() ) != options )
                    fault( _VM_F_Hypercall );
                else
                {
                    int choice = context().choose( options, p.begin(), p.end() );
                    if ( choice >= 0 )
                        result( IntV( choice ) );
                }
                return;
            }

            case lx::HypercallControl:
                return implement_hypercall_control();
            case lx::HypercallSyscall:
                return implement_hypercall_syscall();

            case lx::HypercallInterruptCfl:
                context().cfl_interrupt( pc() );
                return;
            case lx::HypercallInterruptMem:
            {
                auto ptr = operand< PointerV >( 0 );
                if ( ptr.cooked().type() == PointerType::Global &&
                     ptr2s( ptr.cooked() ).location == Slot::Const )
                    return;
                /* TODO fault on failing pointers? */
                auto size = operandCk< IntV >( 1 ).cooked();
                if ( boundcheck( ptr, size, false ) )
                    context().mem_interrupt( pc(), ptr.cooked(), size,
                                             operandCk< IntV >( 2 ).cooked() );
                return;
            }
            case lx::HypercallTrace:
            {
                _VM_Trace t = _VM_Trace( operandCk< IntV >( 0 ).cooked() );
                switch ( t )
                {
                    case _VM_T_Text:
                        context().trace( TraceText{ ptr2h( operandCk< PointerV >( 1 ) ) } );
                        return;
                    case _VM_T_StateType:
                        context().trace( TraceStateType{ pc() } );
                        return;
                    case _VM_T_SchedChoice:
                        context().trace( TraceSchedChoice{
                                PointerV( ptr2h( operandCk< PointerV >( 1 ) ) ) } );
                        return;
                    case _VM_T_SchedInfo:
                        context().trace( TraceSchedInfo{ operandCk< IntV >( 1 ).cooked(),
                                                         operandCk< IntV >( 2 ).cooked() } );
                        return;
                    case _VM_T_Info:
                        context().trace( TraceInfo{ ptr2h( operandCk< PointerV >( 1 ) ) } );
                        return;
                    case _VM_T_Alg: {
                        brick::data::SmallVector< GenericPointer > args;
                        int argc = instruction().argcount() - 1;
                        for ( int i = 1; i < argc; ++i )
                            args.emplace_back( ptr2h( operandCk< PointerV >( i ) ) );
                        context().trace( TraceAlg{ args } );
                        break;
                    }
                    case _VM_T_TypeAlias:
                        context().trace( TraceTypeAlias{
                            pc(), ptr2h( operandCk< PointerV >( 2 ) )
                        } );
                        break;
                    case _VM_T_DebugPersist:
                        context().trace( TraceDebugPersist{ operandCk< PointerV >( 1 ).cooked() } );
                        return;
                    default:
                        fault( _VM_F_Hypercall ) << "invalid __vm_trace type " << t;
                }
                return;
            }

            case lx::HypercallFault:
                fault( Fault( operandCk< IntV >( 0 ).cooked() ) ) << "__vm_fault called";
                return;

            case lx::HypercallObjMake:
            {
                int64_t size = operandCk< IntV >( 0 ).cooked();
                if ( size >= ( 2ll << _VM_PB_Off ) || size < 1 )
                {
                    fault( _VM_F_Hypercall ) << "invalid size " << size
                                             << " passed to __vm_obj_make";
                    size = 0;
                }
                result( size ? makeobj( size ) : nullPointerV() );
                return;
            }
            case lx::HypercallObjFree:
            {
                auto ptr = operandCk< PointerV >( 0 ).cooked();
                if ( !ptr.heap() )
                    fault( _VM_F_Memory ) << "non-heap pointer passed to __vm_obj_free";
                else if ( !freeobj( ptr ) )
                    fault( _VM_F_Memory ) << "invalid pointer passed to __vm_obj_free";
                return;
            }
            case lx::HypercallObjShared:
                heap().shared( operandCk< PointerV >( 0 ).cooked(), true );
                return;
            case lx::HypercallObjResize:
            {
                auto ptr = operandCk< PointerV >( 0 ).cooked();
                if ( !ptr.heap() )
                    fault( _VM_F_Memory ) << "non-heap pointer passed to __vm_obj_resize";
                else if ( !heap().resize( ptr, operandCk< IntV >( 1 ).cooked() ) )
                    fault( _VM_F_Memory ) << "invalid pointer passed to __vm_obj_resize";
                return;
            }
            case lx::HypercallObjSize:
            {
                auto ptr = operandCk< PointerV >( 0 ).cooked();
                if ( !ptr.heap() || !heap().valid( ptr ) )
                    fault( _VM_F_Hypercall ) << "invalid pointer " << ptr << " passed to __vm_obj_size";
                else
                    result( IntV( heap().size( ptr ) ) );
                return;
            }
            case lx::HypercallObjClone:
            {
                auto ptr = operandCk< PointerV >( 0 ).cooked();
                if ( !ptr.heap() || !heap().valid( ptr ) )
                    fault( _VM_F_Hypercall ) << "invalid pointer " << ptr << " passed to __vm_obj_clone";
                else
                    result( PointerV( heap::clone( heap(), heap(), ptr ) ) );
                return;
            }
            default:
                UNREACHABLE_F( "unknown hypercall %d", instruction().subcode );
        }
    }

    void implement_call( bool invoke )
    {
        auto targetV = operand< PointerV >( invoke ? -3 : -1 );

        if ( !targetV.defined() )
        {
            fault( _VM_F_Control ) << "invalid call on an uninitialised pointer " << targetV;
            return;
        }

        if ( targetV.cooked().type() != PointerType::Code )
        {
            fault( _VM_F_Control ) << "invalid call on a pointer which does not point to code: "
                                   << targetV;
            return;
        }
        CodePointer target = targetV.cooked();

        if ( !target.function() )
        {
            if ( instruction().subcode != Intrinsic::not_intrinsic )
                return implement_intrinsic( instruction().subcode );
            fault( _VM_F_Control ) << "invalid call on an invalid code pointer: "
                                   << target;
            return;
        }

        const auto &function = program().function( target );

        if ( target != program().entry( target ) )
        {
            fault( _VM_F_Control ) << "invalid call on a code pointer which does not point "
                                   << "to a function entry: " << target;
            return;
        }


        /* report problems with the call before pushing the new stackframe */
        const int argcount = instruction().argcount() - ( invoke ? 3 : 1 );

        if ( !function.vararg && argcount > function.argcount )
        {
            fault( _VM_F_Control ) << "too many arguments given to a call: "
                                   << function.argcount << " expected but "
                                   << argcount << " given";
            return;
        }

        if ( argcount < function.argcount ) {
            fault( _VM_F_Control ) << "too few arguments given to a call: "
                                   << function.argcount << " expected but "
                                   << argcount << " given";
            return;
        }

        auto frameptr = makeobj( program().function( target ).framesize );
        auto p = frameptr;
        heap().write_shift( p, PointerV( target ) );
        heap().write_shift( p, PointerV( frame() ) );

        /* Copy arguments to the new frame. */
        for ( int i = 0; i < argcount && i < int( function.argcount ); ++i )
            heap().copy( s2ptr( operand( i ) ),
                         s2ptr( function.instructions[ i ].result(), 0, frameptr.cooked() ),
                         function.instructions[ i ].result().size() );

        if ( function.vararg )
        {
            int size = 0;
            for ( int i = function.argcount; i < argcount; ++i )
                size += operand( i ).size();
            auto vaptr = size ? makeobj( size, 1 ) : nullPointerV();
            auto vaptr_loc = s2ptr( function.instructions[ function.argcount ].result(),
                                    0, frameptr.cooked() );
            heap().write( vaptr_loc, vaptr );
            for ( int i = function.argcount; i < argcount; ++i )
            {
                auto op = operand( i );
                heap().copy( s2ptr( op ), vaptr.cooked(), op.size() );
                heap().skip( vaptr, int( op.size() ) );
            }
        }

        ASSERT_NEQ( instruction().opcode, OpCode::PHI );
        context().sync_pc();
        context().set( _VM_CR_Frame, frameptr.cooked() );
        context().set( _VM_CR_PC, target );
        context().entered( target );
    }

    void implement_dbg_call()
    {
        if ( context().enter_debug() )
            implement_call( false );
    }

    void run()
    {
        context().reset_interrupted();
        do {
            advance();
            dispatch();
        } while ( !context().frame().null() );
    }

    void advance()
    {
        ASSERT_EQ( CodePointer( context().get( _VM_CR_PC ).pointer ), pc() );
        context().count_instruction();
        context().check_interrupt( *this );
        context().set( _VM_CR_PC, program().nextpc( pc() + 1 ) );
        _instruction = &program().instruction( pc() );
    }

    void dispatch() /* evaluate a single instruction */
    {
        /* operation templates */

        auto _icmp = [this] ( auto impl ) -> void {
            this->op< IntegerComparable >( 1, [&]( auto v ) {
                    result( impl( v.get( 1 ), v.get( 2 ) ) ); } );
        };

        auto _icmp_signed = [this] ( auto impl ) -> void {
            this->op< IsIntegral >( 1, [&]( auto v ) {
                    this->result( impl( v.get( 1 ).make_signed(),
                                        v.get( 2 ).make_signed() ) ); } );
        };

        auto _div = [this] ( auto impl ) -> void {
            this->op< IsArithmetic >( 0, [&]( auto v )
                {
                    if ( !v.get( 2 ).defined() || !v.get( 2 ).cooked() )
                    {
                        result( decltype( v.get() )( 0 ) );
                        this->fault( _VM_F_Arithmetic ) << "division by zero or an undefined number";
                    } else
                        this->result( impl( v.get( 1 ), v.get( 2 ) ) );
                } );
        };

        auto _arith = [this] ( auto impl ) -> void {
            this->op< IsArithmetic >( 0, [&]( auto v ) {
                    this->result( impl( v.get( 1 ), v.get( 2 ) ) ); } );
        };

        auto _bitwise = [this] ( auto impl ) -> void {
            this->op< IsIntegral >( 0, [&]( auto v ) {
                    this->result( impl( v.get( 1 ), v.get( 2 ) ) ); } );
        };

        auto _atomicrmw = [this] ( auto impl ) -> void {
            this->op< IsIntegral >( 0, [&]( auto v ) {
                    decltype( v.get() ) edit;
                    auto loc = operand< PointerV >( 0 );
                    if ( !boundcheck( loc, sizeof( typename decltype( v.get() )::Raw ), true ) )
                        return; // TODO: destroy pre-existing register value
                    heap().read( ptr2h( loc ), edit );
                    this->result( edit );
                    heap().write( ptr2h( loc ), impl( edit, v.get( 2 ) ) );
                } );
        };

        auto _fcmp = [&]( auto impl ) -> void {
            this->op< IsFloat >( 1, [&]( auto v ) {
                    this->result( impl( v.get( 1 ), v.get( 2 ) ) ); } );
        };

        /* instruction dispatch */
/*
        for ( int i = 1; i < instruction().values.size(); ++i )
            op< Any >( i, [&]( auto v )
                       { std::cerr << " op[" << i << "] = " << v.get( i ) << std::flush; } );
        std::cerr << " | " << std::flush;
        instruction().op->dump();
*/

        switch ( instruction().opcode )
        {
            case OpCode::GetElementPtr:
                result( operand< PointerV >( 0 ) + compositeOffsetFromInsn(
                            instruction().subcode, 1, instruction().argcount() ) );
                return;

            case OpCode::Select:
            {
                auto select = operand< BoolV >( 0 );

                if ( !select.defined() )
                    fault( _VM_F_Control ) << "select on an undefined value";

                slot_copy( s2ptr( operand( select.cooked() ? 1 : 2 ) ),
                           result(), result().size() );
                /* TODO make the result undefined if !select.defined()? */
                return;
            }

            case OpCode::ICmp:
            {
                switch ( instruction().subcode )
                {
                    case ICmpInst::ICMP_EQ:
                        return _icmp( []( auto a, auto b ) { return a == b; } );
                    case ICmpInst::ICMP_NE:
                        return _icmp( []( auto a, auto b ) { return a != b; } );
                    case ICmpInst::ICMP_ULT:
                        return _icmp( []( auto a, auto b ) { return a < b; } );
                    case ICmpInst::ICMP_UGE:
                        return _icmp( []( auto a, auto b ) { return a >= b; } );
                    case ICmpInst::ICMP_UGT:
                        return _icmp( []( auto a, auto b ) { return a > b; } );
                    case ICmpInst::ICMP_ULE:
                        return _icmp( []( auto a, auto b ) { return a <= b; } );
                    case ICmpInst::ICMP_SLT:
                        return _icmp_signed( []( auto a, auto b ) { return a < b; } );
                    case ICmpInst::ICMP_SGT:
                        return _icmp_signed( []( auto a, auto b ) { return a > b; } );
                    case ICmpInst::ICMP_SLE:
                        return _icmp_signed( []( auto a, auto b ) { return a <= b; } );
                    case ICmpInst::ICMP_SGE:
                        return _icmp_signed( []( auto a, auto b ) { return a >= b; } );
                    default: UNREACHABLE_F( "unexpected icmp op %d", instruction().subcode );
                }
            }


            case OpCode::FCmp:
            {
                bool nan, defined;

                this->op< IsFloat >( 1, [&]( auto v )
                       {
                           nan = v.get( 1 ).isnan() || v.get( 2 ).isnan();
                           defined = v.get( 1 ).defined() && v.get( 2 ).defined();
                       } );

                switch ( instruction().subcode )
                {
                    case FCmpInst::FCMP_FALSE:
                        return result( BoolV( false, defined, false ) );
                    case FCmpInst::FCMP_TRUE:
                        return result( BoolV( true, defined, false ) );
                    case FCmpInst::FCMP_ORD:
                        return result( BoolV( !nan, defined, false ) );
                    case FCmpInst::FCMP_UNO:
                        return result( BoolV( nan, defined, false ) );
                    default: ;
                }

                if ( nan )
                    switch ( instruction().subcode )
                    {
                        case FCmpInst::FCMP_UEQ:
                        case FCmpInst::FCMP_UNE:
                        case FCmpInst::FCMP_UGE:
                        case FCmpInst::FCMP_ULE:
                        case FCmpInst::FCMP_ULT:
                        case FCmpInst::FCMP_UGT:
                            return result( BoolV( true, defined, false ) );
                        case FCmpInst::FCMP_OEQ:
                        case FCmpInst::FCMP_ONE:
                        case FCmpInst::FCMP_OGE:
                        case FCmpInst::FCMP_OLE:
                        case FCmpInst::FCMP_OLT:
                        case FCmpInst::FCMP_OGT:
                            return result( BoolV( false, defined, false ) );
                        default: ;
                    }

                switch ( instruction().subcode )
                {
                    case FCmpInst::FCMP_OEQ:
                    case FCmpInst::FCMP_UEQ:
                        return _fcmp( []( auto a, auto b ) { return a == b; } );
                    case FCmpInst::FCMP_ONE:
                    case FCmpInst::FCMP_UNE:
                        return _fcmp( []( auto a, auto b ) { return a != b; } );

                    case FCmpInst::FCMP_OLT:
                    case FCmpInst::FCMP_ULT:
                        return _fcmp( []( auto a, auto b ) { return a < b; } );

                    case FCmpInst::FCMP_OGT:
                    case FCmpInst::FCMP_UGT:
                        return _fcmp( []( auto a, auto b ) { return a > b; } );

                    case FCmpInst::FCMP_OLE:
                    case FCmpInst::FCMP_ULE:
                        return _fcmp( []( auto a, auto b ) { return a <= b; } );

                    case FCmpInst::FCMP_OGE:
                    case FCmpInst::FCMP_UGE:
                        return _fcmp( []( auto a, auto b ) { return a >= b; } );
                    default:
                        UNREACHABLE_F( "unexpected fcmp op %d", instruction().subcode );
                }
            }

            case OpCode::ZExt:
            case OpCode::FPExt:
            case OpCode::UIToFP:
            case OpCode::FPToUI:
            case OpCode::PtrToInt:
            case OpCode::IntToPtr:
            case OpCode::FPTrunc:
            case OpCode::Trunc:

                return op< Convertible >( 0, 1, [this]( auto v0, auto v1 )
                           {
                               result( decltype( v0.get() )( v1.get( 1 ) ) );
                           } );

            case OpCode::SExt:
            case OpCode::SIToFP:
            case OpCode::FPToSI:

                return op< SignedConvertible >( 0, 1, [this]( auto v0, auto v1 )
                           {
                               result( decltype( v0.get().make_signed() )(
                                           v1.get( 1 ).make_signed() ) );
                           } );

            case OpCode::Br:
                implement_br(); break;
            case OpCode::IndirectBr:
                implement_indirectBr(); break;
            case OpCode::Switch:
                return op< Any >( 1, [this]( auto v ) {
                        PointerV target;
                        for ( int o = 2; o < this->instruction().argcount(); o += 2 )
                        {
                            auto eq = v.get( 1 ) == v.get( o + 1 );
                            if ( eq.cooked() )
                                target = operandPtr( o + 1 );
                        }
                        if ( !target.cooked().object() )
                            target = operandPtr( 1 );
                        if ( !v.get( 1 ).defined() )
                        {
                            fault( _VM_F_Control, frame(), target.cooked() )
                                << "switch on an undefined value";
                            return;
                        }
                        for ( int o = 2; o < this->instruction().argcount(); o += 2 )
                        {
                            auto eq = v.get( 1 ) == v.get( o + 1 );
                            if ( !eq.defined() )
                            {
                                fault( _VM_F_Control, frame(), target.cooked() )
                                    << "comparison result undefined for a switch branch";
                                return;
                            }
                        }
                        return this->jumpTo( target );
                    } );

            case lx::OpHypercall:
                return implement_hypercall();
            case lx::OpDbgCall:
                return implement_dbg_call();
            case OpCode::Call:
                implement_call( false ); break;
            case OpCode::Invoke:
                implement_call( true ); break;
            case OpCode::Ret:
                implement_ret(); break;

            case OpCode::BitCast:
                slot_copy( s2ptr( operand( 0 ) ), result(), result().size() );
                break;

            case OpCode::Load:
                implement_load(); break;
            case OpCode::Store:
                implement_store(); break;
            case OpCode::Alloca:
                implement_alloca(); break;

            case OpCode::AtomicCmpXchg: // { old, changed } = cmpxchg ptr, expected, new
                return op< Any >( 2, [&]( auto v ) {
                        using T = decltype( v.get() );
                        auto ptr = operand< PointerV >( 0 );
                        auto expected = v.get( 2 );
                        auto newval = v.get( 3 );

                        if ( !boundcheck( ptr, operand( 2 ).size(), true ) )
                            return;

                        T oldval;
                        heap().read( ptr2h( ptr ), oldval );
                        auto change = oldval == expected;

                        if ( change.cooked() ) {
                            // undefined if one of the inputs was not defined
                            if ( !change.defined() || !ptr.defined() )
                                newval.defined( false );
                            heap().write( ptr2h( ptr ), newval );
                        }
                        slot_write( result(), oldval, 0 );
                        slot_write( result(), change, sizeof( typename T::Raw ) );
                    } );

            case OpCode::AtomicRMW:
            {
                auto minmax = []( auto cmp ) {
                    return [cmp]( auto v, auto x ) {
                        auto c = cmp( v, x );
                        auto out = c.cooked() ? v : x;
                        if ( !c.defined() )
                            out.defined( false );
                        return out;
                    };
                };
                switch ( instruction().subcode )
                {
                    case AtomicRMWInst::Xchg:
                        return _atomicrmw( []( auto, auto x ) { return x; } );
                    case AtomicRMWInst::Add:
                        return _atomicrmw( []( auto v, auto x ) { return v + x; } );
                    case AtomicRMWInst::Sub:
                        return _atomicrmw( []( auto v, auto x ) { return v - x; } );
                    case AtomicRMWInst::And:
                        return _atomicrmw( []( auto v, auto x ) { return v & x; } );
                    case AtomicRMWInst::Nand:
                        return _atomicrmw( []( auto v, auto x ) { return ~v & x; } );
                    case AtomicRMWInst::Or:
                        return _atomicrmw( []( auto v, auto x ) { return v | x; } );
                    case AtomicRMWInst::Xor:
                        return _atomicrmw( []( auto v, auto x ) { return v ^ x; } );
                    case AtomicRMWInst::UMax:
                        return _atomicrmw( minmax( []( auto v, auto x ) { return v > x; } ) );
                    case AtomicRMWInst::Max:
                        return _atomicrmw( minmax( []( auto v, auto x ) {
                                    return v.make_signed() > x.make_signed(); } ) );
                    case AtomicRMWInst::UMin:
                        return _atomicrmw( minmax( []( auto v, auto x ) { return v < x; } ) );
                    case AtomicRMWInst::Min:
                        return _atomicrmw( minmax( []( auto v, auto x ) {
                                    return v.make_signed() < x.make_signed(); } ) );
                    case AtomicRMWInst::BAD_BINOP:
                        UNREACHABLE_F( "bad binop in atomicrmw" );
                }
            }

            case OpCode::ExtractValue:
                implement_extractvalue(); break;
            case OpCode::InsertValue:
                implement_insertvalue(); break;

            case OpCode::FAdd:
            case OpCode::Add:
                return _arith( []( auto a, auto b ) { return a + b; } );
            case OpCode::FSub:
            case OpCode::Sub:
                return _arith( []( auto a, auto b ) { return a - b; } );
            case OpCode::FMul:
            case OpCode::Mul:
                return _arith( []( auto a, auto b ) { return a * b; } );

            case OpCode::FDiv:
            case OpCode::UDiv:
                return _div( []( auto a, auto b ) { return a / b; } );

            case OpCode::SDiv:
			  return _div( []( auto a, auto b ) {
                              return a.make_signed() / b.make_signed(); } );
            case OpCode::FRem:
            case OpCode::URem:
                return _div( []( auto a, auto b ) { return a % b; } );

            case OpCode::SRem:
                return _div( []( auto a, auto b ) { return a.make_signed() % b.make_signed(); } );

            case OpCode::And:
                return _bitwise( []( auto a, auto b ) { return a & b; } );
            case OpCode::Or:
                return _bitwise( []( auto a, auto b ) { return a | b; } );
            case OpCode::Xor:
                return _bitwise( []( auto a, auto b ) { return a ^ b; } );
            case OpCode::Shl:
                return _bitwise( []( auto a, auto b ) { return a << b; } );
            case OpCode::AShr:
                return _bitwise( []( auto a, auto b ) { return a.make_signed() >> b; } );
            case OpCode::LShr:
                return _bitwise( []( auto a, auto b ) { return a >> b; } );

            case OpCode::Unreachable:
                fault( _VM_F_Control ) << "unreachable executed";
                break;

            case OpCode::Resume:
                NOT_IMPLEMENTED();
                break;

            case OpCode::VAArg:
                {
                    PointerV vaList = operand< PointerV >( 0 ), vaArgs;
                    // note: although the va_list type might not be a pointer (as on x86)
                    // we will use it so, assuming that it will be at least as big as a
                    // pointer (for example, on x86_64 va_list is { i32, i32, i64*, i64* }*)
                    if ( !boundcheck( vaList, PointerBytes, true ) )
                        return;
                    heap().read( ptr2h( vaList ), vaArgs );
                    if ( !boundcheck( vaArgs, result().size(), false ) )
                        return;
                    slot_copy( ptr2h( vaArgs ), result(), result().size() );
                    heap().write( ptr2h( vaList ), PointerV( vaArgs + result().size() ) );
                    break;
                }
                return;

            case lx::OpDbg: break;
            case OpCode::LandingPad: break; /* nothing to do, handled by the unwinder */
            case OpCode::Fence: break; /* noop until we have reordering simulation */

            default:
                UNREACHABLE_F( "unknown opcode %d", instruction().opcode );
        }
    }
};

}

#include <divine/vm/heap.hpp>
#include <divine/vm/dbg-node.hpp>

namespace divine::t_vm
{

using vm::CodePointer;
namespace Intrinsic = ::llvm::Intrinsic;

struct TProgram
{
    struct Slot { enum Location { Invalid }; using Type = int; };
    struct Function { int argcount = 0; };
    struct Instruction {};
};

template< typename Prog >
struct TContext : vm::Context< Prog, vm::MutableHeap<> >
{
    using Super = vm::Context< Prog, vm::MutableHeap<> >;
    vm::Fault _fault;

    void fault( vm::Fault f, vm::HeapPointer, CodePointer )
    {
        _fault = f;
        this->set( _VM_CR_Frame, vm::nullPointer() );
    }

    using Super::trace;
    void trace( std::string s ) { std::cerr << "T: " << s << std::endl; }
    void trace( vm::TraceDebugPersist ) { UNREACHABLE( "debug persist not allowed in unit tests" ); }

    TContext( Prog &p ) : vm::Context< Prog, vm::MutableHeap<> >( p ), _fault( _VM_F_NoFault ) {}
};

#ifdef BRICK_UNITTEST_REG
struct Eval
{
    using IntV = vm::value::Int< 32 >;

    TEST(instance)
    {
        TProgram p;
        TContext< TProgram > c( p );
        vm::Eval< TContext< TProgram >, vm::value::Void > e( c );
    }

    template< typename... Args >
    int testP( std::shared_ptr< vm::Program > p, Args... args )
    {
        TContext< vm::Program > c( *p );
        auto data = p->exportHeap( c.heap() );
        c.set( _VM_CR_Constants, data.first );
        c.set( _VM_CR_Globals , data.second );
        vm::Eval< TContext< vm::Program >, IntV > e( c );
        auto pc = p->functionByName( "f" );
        c.enter( pc, vm::nullPointerV(), args... );
        c.set( _VM_CR_Flags, _VM_CF_KernelMode | _VM_CF_Mask );
        e.run();
        return e._result.cooked();
    }

    template< typename... Args >
    int testF( std::string s, Args... args )
    {
        return testP( c2prog( s ), args... );
    }

    template< typename Build, typename... Args >
    int testLLVM( Build build, llvm::FunctionType *ft = nullptr, Args... args )
    {
        return testP( ir2prog( build, "f", ft ), args... );
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
};
#endif

}
