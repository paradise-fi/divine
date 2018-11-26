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
#include <divine/vm/eval.hpp>
#include <divine/vm/memory.tpp>
#include <brick-tuple>

namespace divine::vm
{

using brick::tuple::pass;

template< typename Ctx, typename _T, typename... Args >
struct V
{
    using T = _T;
    Eval< Ctx > *ev;
    std::tuple< Args... > args;

    V( Eval< Ctx > *ev, Args... args ) : ev( ev ), args( args... ) {}

    T get( lx::Slot s )
    {
        T result;
        pass( result, &T::setup, args );
        ev->slot_read( s, result );
        return result;
    }

    T get( PointerV p )
    {
        T result;
        pass( result, &T::setup, args );
        ev->heap().read( ev->ptr2h( p ), result );
        return result;
    }

    T get( int v ) { return get( ev->instruction().value( v ) ); }
    T arg( int v ) { return get( v + 1 ); }

    template< typename R = T, typename... CArgs > R construct( CArgs &&... cargs )
    {
        using namespace brick::tuple;
        R result( std::forward< CArgs... >( cargs )... );
        pass( result, &R::setup, args );
        return result;
    }

    void set( int v, T t )
    {
        ev->slot_write( ev->instruction().value( v ), t );
    }
};

template< typename Ctx > template< template< typename > class Guard, typename T, typename Op,
          typename... Args >
auto Eval< Ctx >::op( Op _op, Args... args ) -> typename std::enable_if< Guard< T >::value >::type
{
    // std::cerr << "op called on type " << typeid( T ).name() << std::endl;
    // std::cerr << instruction() << std::endl;
    _op( V< Ctx, T, Args... >( this, args... ) );
}

template< typename Ctx > template< template< typename > class Guard, typename T, typename... Args >
void Eval< Ctx >::op( NoOp, Args... )
{
    // instruction().op->dump();
    UNREACHABLE_F( "invalid operation on %s", typeid( T ).name() );
}

template< typename Ctx > template< template< typename > class Guard, typename Op >
void Eval< Ctx >::op( int off1, int off2, Op _op )
{
    op< Any >( off1, [&]( auto v1 ) {
            return this->op< Guard< decltype( v1.construct() ) >::template Guard >(
                off2, [&]( auto v2 ) { return _op( v1, v2 ); } );
        } );
}

template< typename Ctx > template< template< typename > class Guard, typename Op >
void Eval< Ctx >::op( int off, Op _op )
{
    auto v = instruction().value( off );
    return type_dispatch< Guard >( v.type, _op, v );
}

template< typename Ctx > template< template< typename > class Guard, typename Op >
void Eval< Ctx >::type_dispatch( typename Slot::Type type, Op _op, Slot slot )
{
    switch ( type )
    {
        case Slot::I1: return op< Guard, value::Int<  1 > >( _op );
        case Slot::I8: return op< Guard, value::Int<  8 > >( _op );
        case Slot::I16: return op< Guard, value::Int< 16 > >( _op );
        case Slot::I32: return op< Guard, value::Int< 32 > >( _op );
        case Slot::I64: return op< Guard, value::Int< 64 > >( _op );
        case Slot::IX:
            ASSERT( slot.width() );
            return op< Guard, value::DynInt<> >( _op, slot.width() );
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

template< typename Ctx > template< typename V_ >
void Eval< Ctx >::slot_read( Slot s, V_ &v )
{
    heap().read( s2loc( s ), v );
}

template< typename Ctx > template< typename V_ >
void Eval< Ctx >::slot_read( Slot s, HeapPointer fr, V_ &v )
{
    heap().read( s2ptr( s, 0, fr ), v );
}

template< typename Ctx > template< typename T >
T Eval< Ctx >::operand( int i ) { return V< Ctx, T >( this ).get( i >= 0 ? i + 1 : i ); }

template< typename Ctx > template< typename T >
void Eval< Ctx >::result( T t ) { V< Ctx, T >( this ).set( 0, t ); }

template< typename Ctx > template< typename T >
auto Eval< Ctx >::operandCk( int i )
{
    auto op = operand< T >( i );
    if ( !op.defined() )
        fault( _VM_F_Hypercall ) << "operand " << i << " has undefined value: " << op;
    return op;
}

template< typename Ctx > template< typename T >
auto Eval< Ctx >::operandCk( int idx, Instruction &insn, HeapPointer fr )
{
    T val;
    slot_read( insn.operand( idx ), fr, val );
    if ( !val.defined() )
        fault( _VM_F_Hypercall ) << "operand " << idx << " has undefined value: "  << val;
    return val;
}

template< typename Ctx >
typename Eval< Ctx >::FaultStream Eval< Ctx >::fault( Fault f )
{
    if ( frame().null() || !heap().valid( frame() ) )
        return fault( f, nullPointer( PointerType::Heap ), nullPointer( PointerType::Code ) );
    else
        return fault( f, frame(), pc() );
}

template< typename Ctx >
typename Eval< Ctx >::FaultStream Eval< Ctx >::fault( Fault f, HeapPointer frame, CodePointer c )
{
    PointerV fr( frame );
    PointerV fpc;
    while ( !context().debug_mode() && !fr.cooked().null() && heap().valid( fr.cooked() ) )
    {
        heap().read_shift( fr, fpc );
        if ( fpc.cooked().object() == context().fault_handler().object() )
            return FaultStream( context(), f, frame, c, true, true );
        heap().read( fr.cooked(), fr );
    }

    context().sync_pc();
    FaultStream fs( context(), f, frame, c, true, false );
    return fs;
}

template< typename Ctx > template< typename MkF >
bool Eval< Ctx >::boundcheck( MkF mkf, PointerV p, int sz, bool write, std::string dsc )
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

template< typename Ctx >
auto Eval< Ctx >::gep( int type, int idx, int end ) -> IntV // getelementptr
{
    if ( idx == end )
        return IntV( 0 );

    int64_t min = std::numeric_limits< int >::min(),
            max = std::numeric_limits< int >::max();

    value::Int< 64, true > offset;
    auto fetch = [&]( auto v ) { offset = v.get( idx + 1 ).make_signed(); };
    type_dispatch< IsIntegral >( operand( idx ).type, fetch, operand( idx ) );

    if ( offset.cooked() < min || offset.cooked() > max )
        return IntV( 0, 0, false );

    auto subtype = types().subtype( type, offset.cooked() );
    auto offset_sub = gep( subtype.second, idx + 1, end );
    int64_t a = offset_sub.cooked(), b = subtype.first;

    if ( a > 0 && b > 0 && ( b > max - a || a + b > max ) ||
         a < 0 && b < 0 && ( b < min - a || a + b < min ) )
        return IntV( 0, 0, false ); /* undefined */

    IntV offset_bytes( subtype.first );
    offset_bytes.defined( offset.defined() );

    return IntV( offset_bytes ) + offset_sub;
}

template< typename Ctx >
void Eval< Ctx >::implement_extractvalue()
{
    auto off = gep( instruction().subcode, 1, instruction().argcount() );
    ASSERT( off.defined() );
    slot_copy( s2ptr( operand( 0 ), off.cooked() ), result(), result().size() );
}

template< typename Ctx >
void Eval< Ctx >::implement_insertvalue()
{
    /* first copy the original */
    slot_copy( s2ptr( operand( 0 ) ), result(), result().size() );
    auto off = gep( instruction().subcode, 2, instruction().argcount() );
    ASSERT( off.defined() );
    /* write the new value over the selected field */
    slot_copy( s2ptr( operand( 1 ) ), result(), operand( 1 ).size(), off.cooked() );
}

template< typename Ctx > template< typename F >
void Eval< Ctx >::each_phi( CodePointer first, F f )
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

template< typename Ctx >
void Eval< Ctx >::switchBB( CodePointer target )
{
    auto origin = pc();
    context().set( _VM_CR_PC, target );

    if ( !target.function() || program().instruction( target ).opcode != lx::OpBB )
        return;

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
                    size += brick::bitlevel::align( i.result().size(), 4 );
                    ++ count;
                } );
    auto tmp = makeobj( size ), tgt = tmp;
    each_phi( target, [&]( auto &i )
                {
                    heap().copy( s2ptr( i.operand( idx ) ), tgt.cooked(), i.result().size() );
                    heap().skip( tgt, brick::bitlevel::align( i.result().size(), 4 ) );
                } );
    tgt = tmp;
    each_phi( target, [&]( auto &i )
                {
                    slot_copy( tgt.cooked(), i.result(), i.result().size() );
                    heap().skip( tgt, brick::bitlevel::align( i.result().size(), 4 ) );
                } );

    freeobj( tmp.cooked() );
    target.instruction( target.instruction() + count - 1 );
    context().set( _VM_CR_PC, target );
}

template< typename Eval, typename F, typename Y >
static void collect_results( Eval &eval, Program::Function &f, F filter, Y yield )
{
    for ( auto &i : f.instructions )
        if ( filter( i ) )
        {
            PointerV ptr;
            eval.slot_read( i.result(), ptr );
            if ( eval.heap().valid( ptr.cooked() ) )
                yield( ptr, i.result() );
        }
}

template< typename Eval, typename F, typename Y >
static void collect_results( Eval &eval, CodePointer pc, F filter, Y yield )
{
    auto &f = eval.program().functions[ pc.function() ];
    collect_results( eval, f, filter, yield );
}

template< typename Ctx > template< typename Y >
void Eval< Ctx >::collect_allocas( CodePointer pc, Y yield )
{
    collect_results( *this, pc, []( auto &i ) { return i.opcode == OpCode::Alloca; }, yield );
}

template< typename Ctx > template< typename Y >
void Eval< Ctx >::collect_frame_locals( CodePointer pc, Y yield )
{
    auto &f = program().functions[ pc.function() ];
    if ( f.vararg )
    {
        auto vaptr_loc = f.instructions[ f.argcount ].result();
        PointerV ptr;
        slot_read( vaptr_loc, ptr );
        if ( !ptr.cooked().null() )
            yield( ptr, vaptr_loc );
    }
    collect_results( *this, f, []( auto &i )
        {
            return i.opcode == OpCode::Alloca
                || (i.opcode == OpCode::Call && i.subcode == Intrinsic::stacksave );
        }, yield );
}

template< typename Ctx >
void Eval< Ctx >::implement_indirectBr()
{
    local_jump( operandCk< PointerV >( 0 ) );
}

template< typename Ctx >
void Eval< Ctx >::implement_stacksave()
{
    std::vector< PointerV > ptrs;
    collect_allocas( pc(), [&]( PointerV p, auto ) { ptrs.push_back( p ); } );
    auto r = makeobj( sizeof( IntV::Raw ) + ptrs.size() * PointerBytes );
    auto p = r;
    heap().write_shift( p, IntV( ptrs.size() ) );
    for ( auto ptr : ptrs )
        heap().write_shift( p, ptr );
    result( r );
}

template< typename Ctx >
void Eval< Ctx >::implement_stackrestore()
{
    auto r = operand< PointerV >( 0 );

    if ( !boundcheck( r, sizeof( int ), false ) )
        return;

    IntV count;
    heap().read_shift( r, count );
    if ( !count.defined() )
        fault( _VM_F_Hypercall ) << " stackrestore with undefined count";
    auto s = r;

    collect_allocas( pc(), [&]( auto ptr, auto slot )
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
        {
            freeobj( ptr.cooked() );
            slot_write( slot, PointerV( nullPointer() ) );
        }
    } );
}

template< typename Ctx >
void Eval< Ctx >::implement_intrinsic( int id )
{
    auto _arith_with_overflow = [this]( auto wrap, auto impl, auto check ) -> void
    {
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

    switch ( id )
    {
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
                                         []( auto a, auto b )
                                         {
                                             return (a > _maxbound( a ) / b)
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
        case Intrinsic::lifetime_start:
        {
            auto size = operand< IntV >( 0 );
            auto op = operand< PointerV >( 1 );
            if ( !heap().valid( op.cooked() ) )
                heap().make( size.cooked(), op.cooked().object(), true );
            return;
        }
        case Intrinsic::lifetime_end:
        {
            auto op = operand< PointerV >( 1 );
            if ( !freeobj( op.cooked() ) )
                fault( _VM_F_Memory ) << "invalid pointer passed to llvm.lifetime.end";
            return;
        }
        default:
            /* We lowered everything else in buildInfo. */
            // instruction().op->dump();
            UNREACHABLE_F( "unexpected intrinsic %d", id );
    }
}

template< typename Ctx >
void Eval< Ctx >::update_shuffle()
{
    using brick::bitlevel::mixdown;
    uint64_t next = mixdown( heap().objhash( context().ptr2i( _VM_Operand::Location( _VM_CR_Frame ) ) ),
                             context().frame().object() );
    context().set( _VM_CR_ObjIdShuffle, next );
}

template< typename Ctx >
void Eval< Ctx >::implement_hypercall_syscall()
{
    int idx = 0;

    std::vector< long > args;
    std::vector< bool > argt;
    std::vector< std::unique_ptr< char[] > > bufs;

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
            bufs.emplace_back( size ? new char[ size ] : nullptr );
            args.push_back( long( bufs.back().get() ) );
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
}

template< typename Ctx >
void Eval< Ctx >::implement_hypercall()
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

        case lx::HypercallCtlSet:  return implement_ctl_set();
        case lx::HypercallCtlGet:  return implement_ctl_get();
        case lx::HypercallCtlFlag: return implement_ctl_flag();

        case lx::HypercallTestLoop: return implement_test_loop();
        case lx::HypercallTestCrit: return implement_test_crit();
        case lx::HypercallTestTaint: return implement_test_taint();

        case lx::HypercallPeek: return implement_peek();
        case lx::HypercallPoke: return implement_poke();

        case lx::HypercallSyscall:
            return implement_hypercall_syscall();

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
                case _VM_T_TaskID:
                    context().trace( TraceTaskID{ operandCk< PointerV >( 1 ).cooked() } );
                    return;
                case _VM_T_Info:
                    context().trace( TraceInfo{ ptr2h( operandCk< PointerV >( 1 ) ) } );
                    return;
                case _VM_T_Assume:
                    context().trace( TraceAssume{ ptr2h( operandCk< PointerV >( 1 ) ) } );
                    return;
                case _VM_T_LeakCheck:
                    context().trace( TraceLeakCheck() );
                    return;
                case _VM_T_TypeAlias:
                    context().trace( TraceTypeAlias{ pc(), ptr2h( operandCk< PointerV >( 2 ) ) } );
                    return;
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
                result( nullPointerV() );
                fault( _VM_F_Hypercall ) << "invalid size " << size
                                            << " passed to __vm_obj_make";
                return;
            }
            result( size ? makeobj( size ) : nullPointerV() );
            return;
        }
        case lx::HypercallObjFree:
        {
            auto op = operand< PointerV >( 0 );
            auto ptr = op.cooked();
            if ( !op.defined() )
                fault( _VM_F_Memory ) << "undefined pointer passed to __vm_obj_free";
            else if ( !ptr.heap() )
                fault( _VM_F_Memory ) << "non-heap pointer passed to __vm_obj_free";
            else if ( !freeobj( ptr ) )
                fault( _VM_F_Memory ) << "invalid pointer passed to __vm_obj_free";
            return;
        }
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
                result( PointerV( mem::clone( heap(), heap(), ptr ) ) );
            return;
        }
        default:
            UNREACHABLE_F( "unknown hypercall %d", instruction().subcode );
    }
}

template< typename Ctx >
void Eval< Ctx >::implement_call( bool invoke )
{
    auto targetV = operand< PointerV >( invoke ? -3 : -1 );

    if ( !targetV.defined() )
    {
        fault( _VM_F_Control ) << "invalid call on an uninitialised pointer " << targetV;
        return;
    }

    if ( ! targetV.cooked().null() && targetV.cooked().type() != PointerType::Code )
    {
        fault( _VM_F_Control ) << "invalid call on a pointer which does not point to code: "
                                << targetV;
        return;
    }
    CodePointer target = targetV.cooked();

    ASSERT( !(!invoke && target.function()) || instruction().subcode == Intrinsic::not_intrinsic );

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

    if ( argcount < function.argcount )
    {
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

template< typename Ctx >
void Eval< Ctx >::implement_ctl_set_frame()
{
    if ( instruction().argcount() > 3 )
    {
        fault( _VM_F_Hypercall ) << "too many arguments to __vm_ctl_set";
        return;
    }

    auto ptr = operandCk< PointerV >( 1 );

    if ( !ptr.cooked().null() && !boundcheck_nop( ptr, 2 * PointerBytes, true ) )
    {
        fault( _VM_F_Hypercall ) << "invalid target frame in __vm_ctl_set";
        return;
    }

    bool free = true;
    context().sync_pc();

    if ( context().flags_all( _VM_CF_KeepFrame ) )
    {
        context().flags_set( _VM_CF_KeepFrame, 0 );
        free = false;
    }
    else if ( ptr.cooked() == GenericPointer( frame() ) )
    {
        fault( _VM_F_Hypercall, GenericPointer(), CodePointer() )
            << " cannot target current frame without _VM_CF_KeepFrame";
        return;
    }

    auto caller_pc = pc();
    auto maybe_free = [&]
    {
        if ( !free ) return;
        collect_frame_locals( caller_pc, [&]( auto ptr, auto ) { freeobj( ptr.cooked() ); } );
        freeobj( frame() );
    };

    if ( ptr.cooked().null() )
    {
        maybe_free();
        return context().set( _VM_CR_Frame, ptr.cooked() );
    }

    PointerV oldpc;
    heap().read( ptr.cooked(), oldpc );
    context().set( _VM_CR_PC, oldpc.cooked() );

    if ( instruction().argcount() == 3 )
    {
        auto target = operandCk< PointerV >( 2 );
        maybe_free();
        context().set( _VM_CR_Frame, ptr.cooked() );
        long_jump( target );
    }
    else if ( !ptr.cooked().null() )
    {
        auto opcode = [&]( int off ) { return program().instruction( oldpc.cooked() + off ).opcode; };

        if ( opcode( 0 ) == OpCode::PHI )
        {
            fault( _VM_F_Hypercall ) << "cannot transfer control into the middle of a phi block";
            return;
        }

        if ( opcode( 0 ) == lx::OpBB && opcode( 1 ) == OpCode::PHI )
        {
            fault( _VM_F_Hypercall ) << "cannot transfer control directly to a basic block w/ phi nodes";
            return;
        }

        maybe_free();
        context().set( _VM_CR_Frame, ptr.cooked() );
    }

    update_shuffle();
}

template< typename Ctx >
void Eval< Ctx >::implement_ctl_set()
{
    auto reg = _VM_ControlRegister( operandCk< IntV >( 0 ).cooked() );

    switch ( reg ) /* permission check */
    {
        case _VM_CR_Flags:
        case _VM_CR_Globals:
            if ( !assert_flag( _VM_CF_KernelMode, "cannot change register in user mode" ) )
                return;
            else break;
        case _VM_CR_State:
        case _VM_CR_Scheduler:
        case _VM_CR_FaultHandler:
            if ( !assert_flag( _VM_CF_Booting, "can only change register during boot" ) )
                return;
            else break;
        case _VM_CR_Constants:
        case _VM_CR_ObjIdShuffle:
            fault( _VM_F_Access ) << "attempted to change (immutable) control register " << reg;
            return;
        default: break;
    }

    if ( reg == _VM_CR_Frame )
        return implement_ctl_set_frame();

    if ( instruction().argcount() > 2 )
    {
        fault( _VM_F_Hypercall ) << "too many arguments to __vm_ctl_set";
        return;
    }

    if ( reg == _VM_CR_Flags )
    {
        uint64_t want = operandCk< PtrIntV >( 1 ).cooked();
        uint64_t change = context().flags() ^ want;
        if ( change & _VM_CF_DebugMode )
        {
            fault( _VM_F_Access ) << "debug mode cannot be changed";
            return;
        }
        context().set( reg, operandCk< PtrIntV >( 1 ).cooked() );
    }
    else
        context().set( reg, operandCk< PointerV >( 1 ).cooked() );

}

template< typename Ctx >
void Eval< Ctx >::implement_ctl_get()
{
    auto reg = _VM_ControlRegister( operandCk< IntV >( 0 ).cooked() );

    switch ( reg ) /* permission check */
    {
        case _VM_CR_Globals:
        case _VM_CR_Constants:
        case _VM_CR_State:
        case _VM_CR_Scheduler:
        case _VM_CR_ObjIdShuffle:
            if ( !assert_flag( _VM_CF_KernelMode, "register " + brick::string::fmt( reg ) +
                               " only readable in kernel mode" ) )
                return;
            else break;
        default: break;
    }

    /* TODO mask out privileged bits from _VM_CR_Flags? */
    if ( reg == _VM_CR_Flags )
        result( PtrIntV( context().flags() ) );
    else
        result( PointerV( context().get_ptr( reg ) ) );
}

template< typename Ctx >
void Eval< Ctx >::implement_ctl_flag()
{
    uint64_t clear = operandCk< PtrIntV >( 0 ).cooked(),
               set = operandCk< PtrIntV >( 1 ).cooked(),
            change = clear | set;

    if ( set & _VM_CF_KernelMode )
        if ( !program().traps( pc() ) )
        {
            fault( _VM_F_Access ) << "cannot enter kernel mode here";
            return;
        }

    if ( set & _VM_CF_Booting )
    {
        fault( _VM_F_Access ) << "the 'booting' flag cannot be changed";
        return;
    }

    if ( change & _VM_CF_DebugMode )
    {
        fault( _VM_F_Access ) << "the 'debug' flag cannot be changed";
        return;
    }

    if ( change & _VM_CF_Error )
        if ( !assert_flag( _VM_CF_KernelMode, "the error flag can be only changed in kernel mode" ) )
            return;

    result( PtrIntV( context().flags() ) );
    context().flags_set( clear, set );
}

template< typename Ctx >
void Eval< Ctx >::implement_test_loop()
{
    auto counter = operandCk< IntV >( 0 ).cooked();

    if ( !context().test_loop( pc(), counter ) )
        return;

    context().sync_pc();
    CodePointer h = operandCk< PointerV >( 1 ).cooked();
    context().enter( h, PointerV( frame() ) );
}

template< typename Ctx >
void Eval< Ctx >::implement_test_crit()
{
    auto ptr = operand< PointerV >( 0 );

    if ( ptr.cooked().type() == PointerType::Global && ptr2s( ptr.cooked() ).location == Slot::Const )
        return;

    auto size = operandCk< IntV >( 1 ).cooked();

    if ( !boundcheck( ptr, size, false ) )
    {
        std::cerr << "W: boundcheck failed on vm.test.crit" << std::endl;
        return; /* TODO fault on failing pointers? */
    }

    auto at = operandCk< IntV >( 2 ).cooked();

    if ( !context().test_crit( pc(), ptr.cooked(), size, at ) )
        return;

    context().sync_pc();
    CodePointer h = operandCk< PointerV >( 3 ).cooked();
    context().enter( h, PointerV( frame() ) );
}

template< typename Ctx >
void Eval< Ctx >::implement_test_taint()
{
    CodePointer tf = operandCk< PointerV >( 0 ).cooked();
    CodePointer nf;

    if ( operand( 1 ).type == Slot::PtrC )
        nf = operandCk< PointerV >( 1 ).cooked();

    uint8_t taints = 0;
    for ( int i = 2; i < instruction().argcount(); ++i )
        op< Any >( i + 1, [&]( auto v ) { taints |= v.arg( i ).taints(); } );

    context().sync_pc();
    auto oldframe = frame();
    context()._incremental_enter = true;

    if ( taints )
        context().enter( tf, PointerV( frame() ) );
    else if ( nf.function() ) /* what to call for non-tainted */
        context().enter( nf, PointerV( frame() ) );
    else
    {
        slot_copy( s2ptr( operand( 1 ) ), result(), result().size() );
        context()._incremental_enter = false;
        return;
    }

    auto newframe = frame();
    auto &ff = program().function( taints ? tf : nf );

    for ( int i = 2; i < instruction().argcount(); ++i )
        op< Any >( i + 1, [&]( auto v )
        {
            context().set( _VM_CR_Frame, oldframe );
            auto arg = v.arg( i );
            context().set( _VM_CR_Frame, newframe );
            auto taint_v = value::Int< 8 >( arg.taints() );
            if ( taints )
                context().push( ff, (i - 2) * 2, frame(), taint_v, arg );
            else
                context().push( ff, (i - 2), frame(), arg );
        } );
}

template< typename Ctx >
void Eval< Ctx >::implement_peek()
{
    auto ptr = operandCk< PointerV >( 0 );
    if ( !ptr.defined() || !boundcheck( ptr, 1, false ) )
        return;
    int key = operandCk< IntV >( 1 ).cooked();
    auto loc = heap().loc( ptr2h( ptr ) );
    switch ( key )
    {
        case _VM_ML_Pointers:
        {
            auto ptrs = heap().pointers( loc, 1 );
            GenericPointer::ObjT obj = 0;

            for ( auto p : ptrs ) /* there's at most 1 */
                switch ( p.size() )
                {
                    case 1: obj = p.fragment(); break;
                    default: NOT_IMPLEMENTED();
                }
            return result( IntV( obj ) );
        }
        default:
            result( heap().peek( loc, key - _VM_ML_User ) );
    }
}

template< typename Ctx >
void Eval< Ctx >::implement_poke()
{
    auto ptr = operandCk< PointerV >( 0 );
    if ( !ptr.defined() || !boundcheck( ptr, 1, true ) )
        return;
    HeapPointer where = ptr2h( ptr );
    int layer = operandCk< IntV >( 1 ).cooked();
    auto value = operandCk< value::Int< 32, false > >( 2 );
    auto loc = heap().loc( where );

    switch ( layer )
    {
        case _VM_ML_Taints:
        {
            IntV data;
            heap().read( where, data );
            data.taints( value.cooked() );
            heap().write( where, data );
            break;
        }
        default:
            ASSERT( layer >= _VM_ML_User );
            heap().poke( loc, layer - _VM_ML_User, value );
    }
}

template< typename Ctx >
void Eval< Ctx >::implement_ret()
{
    PointerV fr( frame() );
    heap().skip( fr, sizeof( typename PointerV::Raw ) );
    PointerV parent, br;
    heap().read( fr.cooked(), parent );

    collect_frame_locals( pc(), [&]( auto ptr, auto ) { freeobj( ptr.cooked() ); } );

    if ( parent.cooked().null() )
    {
        if ( context().flags_any( _VM_CF_AutoSuspend ) )
        {
            context().flags_set( 0, _VM_CF_KeepFrame );
            _final_frame = frame();
            context().set( _VM_CR_Frame, parent.cooked() );
        }
        else
            fault( _VM_F_Control ) << "trying to return without a caller";
        return;
    }

    PointerV caller_pc;
    heap().read( parent.cooked(), caller_pc );
    const auto &caller = program().instruction( caller_pc.cooked() );
/* TODO
    if ( caller.opcode != OpCode::Invoke && caller.opcode != OpCode::Call )
    {
        fault( _VM_F_Control, parent.cooked(), caller_pc.cooked() )
            << "Illegal return to a non-call location.";
        return;
    }
*/
    if ( instruction().argcount() ) /* return value */
    {
        if ( !caller.has_result() ) {
            fault( _VM_F_Control, parent.cooked(), caller_pc.cooked() )
                << "Function which was called as void returned a value";
            return;
        } else if ( caller.result().size() != operand( 0 ).size() ) {
            fault( _VM_F_Control, parent.cooked(), caller_pc.cooked() )
                << "The returned value is bigger than expected by the caller";
            return;
        } else if ( !heap().copy( s2ptr( operand( 0 ) ),
                                s2ptr( caller.result(), 0, parent.cooked() ),
                                caller.result().size() ) )
        {
            fault( _VM_F_Memory, parent.cooked(), caller_pc.cooked() )
                << "Could not return a value";
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
        local_jump( br );
    }
    freeobj( fr.cooked() );
    context().left( retpc );
}

template< typename Ctx >
void Eval< Ctx >::implement_br()
{
    if ( instruction().argcount() == 1 )
        local_jump( operandCk< PointerV >( 0 ) );
    else
    {
        auto cond = operand< BoolV >( 0 );
        auto target = operandCk< PointerV >( cond.cooked() ? 2 : 1 );
        if ( cond.defbits() & 1 )
            local_jump( target );
        else
            fault( _VM_F_Control, frame(), target.cooked() )
                << " conditional jump depends on an undefined value";
    }
}

template< typename Ctx >
void Eval< Ctx >::dispatch() /* evaluate a single instruction */
{
    /* operation templates */

    auto _icmp = [this] ( auto impl ) -> void
    {
        op< IntegerComparable >( 1, [&]( auto v ) { result( impl( v.get( 1 ), v.get( 2 ) ) ); } );
    };

    auto _icmp_signed = [this] ( auto impl ) -> void
    {
        op< IsIntegral >( 1, [&]( auto v ) {
                this->result( impl( v.get( 1 ).make_signed(),
                                    v.get( 2 ).make_signed() ) ); } );
    };

    auto _div = [this] ( auto impl ) -> void
    {
        op< IsArithmetic >( 0, [&]( auto v )
            {
                if ( !v.get( 2 ).defined() || !v.get( 2 ).cooked() )
                {
                    constexpr bool is_float = std::is_floating_point<
                                                decltype( v.get( 2 ).cooked() ) >::value;
                    if constexpr ( is_float )
                        // for floats, the errors can be ignored, as they do
                        // not raise signals normally, so calculate the value
                        this->result( impl( v.get( 1 ), v.get( 2 ) ) );
                    else
                    {
                        auto rv = v.get( 2 );
                        rv.taints( rv.taints() | v.get( 1 ).taints() );
                        result( rv );
                    }
                    this->fault( is_float ? _VM_F_Float : _VM_F_Integer )
                          << "division by " << v.get( 2 );
                } else
                    this->result( impl( v.get( 1 ), v.get( 2 ) ) );
            } );
    };

    auto _arith = [this] ( auto impl ) -> void
    {
        op< IsArithmetic >( 0, [&]( auto v ) {
                this->result( impl( v.get( 1 ), v.get( 2 ) ) ); } );
    };

    auto _bitwise = [this] ( auto impl ) -> void
    {
        op< IsIntegral >( 0, [&]( auto v ) {
                this->result( impl( v.get( 1 ), v.get( 2 ) ) ); } );
    };

    auto _atomicrmw = [this] ( auto impl ) -> void
    {
        op< IsIntegral >( 0, [&]( auto v ) {
                auto edit = v.construct();
                auto loc = operand< PointerV >( 0 );
                if ( !boundcheck( loc, sizeof( typename decltype( v.construct() )::Raw ), true ) )
                    return; // TODO: destroy pre-existing register value
                heap().read( ptr2h( loc ), edit );
                this->result( edit );
                heap().write( ptr2h( loc ), impl( edit, v.get( 2 ) ) );
            } );
    };

    auto _fcmp = [&]( auto impl ) -> void
    {
        op< IsFloat >( 1, [&]( auto v ) {
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
            result( operand< PointerV >( 0 ) +
                    gep( instruction().subcode, 1, instruction().argcount() ) );
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

            op< IsFloat >( 1, [&]( auto v )
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
                            result( v0.construct( v1.get( 1 ) ) );
                        } );

        case OpCode::SExt:
        case OpCode::SIToFP:
        case OpCode::FPToSI:

            return op< SignedConvertible >( 0, 1, [this]( auto v0, auto v1 )
                        {
                            using Signed = decltype( v0.construct().make_signed() );
                            result( v0.template construct< Signed >( v1.get( 1 ).make_signed() ) );
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
                    return this->local_jump( target );
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
                    using T = decltype( v.construct() );
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

        case OpCode::LandingPad: break; /* nothing to do, handled by the unwinder */
        case OpCode::Fence: break; /* noop until we have reordering simulation */

        default:
            UNREACHABLE_F( "unknown opcode %d", instruction().opcode );
    }
}

template< typename Ctx >
void Eval< Ctx >::run()
{
    context().reset_interrupted();
    do {
        advance();
        dispatch();
    } while ( !context().frame().null() );
}

template< typename Ctx >
bool Eval< Ctx >::run_seq( bool continued )
{
    if ( continued )
        dispatch();
    else
        context().reset_interrupted();

    do {
        advance();
        if ( instruction().opcode == lx::OpHypercall && instruction().subcode == lx::HypercallChoose )
            return true;
        dispatch();
    } while ( !context().frame().null() );

    return false;
}

}

// vim: syntax=cpp tabstop=4 shiftwidth=4 expandtab ft=cpp
