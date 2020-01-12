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
#include <divine/vm/opnames.hpp>
#include <divine/vm/ctx-debug.tpp>

#include <divine/vm/eval-slot.tpp>
#include <divine/vm/eval-ops.tpp>
#include <divine/vm/eval-hyper.tpp>
#include <divine/vm/eval-intrin.tpp>
#include <divine/vm/eval-bounds.tpp>

namespace divine::vm
{

using brick::tuple::pass;

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
FaultStream< Ctx > Eval< Ctx >::fault( Fault f )
{
    if ( frame().null() || !heap().valid( frame() ) )
        return fault( f, nullPointer(), nullPointer() );
    else
        return fault( f, frame(), pc() );
}

template< typename Ctx >
FaultStream< Ctx > Eval< Ctx >::fault( Fault f, HeapPointer frame, CodePointer c )
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

template< typename Ctx >
auto Eval< Ctx >::gep( int type, int idx, int end ) -> value::Int< 64, true > // getelementptr
{
    if ( idx == end )
        return value::Int< 64, true >( 0 );

    int64_t min = std::numeric_limits< int64_t >::min(),
            max = std::numeric_limits< int64_t >::max();

    value::Int< 64, true > offset;
    auto fetch = [&]( auto v ) { offset = v.get( idx + 1 ).make_signed(); };
    type_dispatch< IsIntegral >( operand( idx ).type, fetch, operand( idx ) );

    auto subtype = types().subtype( type, offset.cooked() );
    auto offset_sub = gep( subtype.second, idx + 1, end );
    int64_t a = offset_sub.cooked(), b = subtype.first;

    if ( a > 0 && b > 0 && ( b > max - a || a + b > max ) ||
         a < 0 && b < 0 && ( b < min - a || a + b < min ) )
        return { 0, 0, false }; /* undefined */

    value::Int< 64, true > offset_bytes( subtype.first );
    offset_bytes.defined( offset.defined() );

    return offset_bytes + offset_sub;
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

/* Two-phase PHI handler. We do this because all of the PHI nodes must be
 * executed atomically, reading their inputs before any of the results are
 * updated.  Not doing this can cause problems if the PHI nodes depend on
 * other PHI nodes for their inputs.  If the input PHI node is updated
 * before it is read, incorrect results can happen. */

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

    auto &f = program().functions[ target.function() ];
    std::unordered_set< int > read_offsets;
    bool onephase = true;
    PointerV stash_obj, stash_ptr;

    auto count_phis = [&]( auto &i )
    {
        size += brick::bitlevel::align( i.result().size(), 4 );
        read_offsets.insert( i.operand( idx ).offset );
        ++ count;
    };

    auto check_overlap = [&]( auto &i )
    {
        if ( read_offsets.count( i.result().offset ) )
            onephase = false;
    };

    auto stash = [&]( auto &i )
    {
        heap().copy( s2ptr( i.operand( idx ) ), stash_ptr.cooked(), i.result().size() );
        heap().skip( stash_ptr, brick::bitlevel::align( i.result().size(), 4 ) );
    };

    auto unstash = [&]( auto &i )
    {
        slot_copy( stash_ptr.cooked(), i.result(), i.result().size() );
        heap().skip( stash_ptr, brick::bitlevel::align( i.result().size(), 4 ) );
    };

    auto direct = [&]( auto &i )
    {
        slot_copy( s2ptr( i.operand( idx ) ), i.result(), i.result().size() );
    };

    each_phi( target, count_phis );
    each_phi( target, check_overlap );

    if ( onephase )
        each_phi( target, direct );
    else
    {
        stash_obj = makeobj( size ), stash_ptr = stash_obj;
        each_phi( target, stash );
        stash_ptr = stash_obj;
        each_phi( target, unstash );
        freeobj( stash_obj.cooked() );
    }

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
void Eval< Ctx >::update_shuffle()
{
    using brick::bitlevel::mixdown;
    auto [ data, ptr ] = heap().hash_data( context().ptr2i( _VM_Operand::Location( _VM_CR_Frame ) ) );
    uint64_t next = mixdown( data ^ ptr, context().frame().object() );
    context().set( _VM_CR_ObjIdShuffle, next );
}

template< typename Ctx >
void Eval< Ctx >::dispatch() /* evaluate a single instruction */
{
    auto _icmp_impl = [&]( auto check, auto cmp ) -> void
    {
        auto impl = [&]( auto v )
        {
            auto a = v.get( 1 ), b = v.get( 2 );
            result( cmp( a, b ) );
            check( a, b );
        };
        op< IntegerComparable >( 1, impl );
    };

    auto has_objid = []( auto p ) { return p.objid() && p.pointer(); };
    auto is_null = []( auto p ) { return p.objid() == 0; };

    auto _icmp_chk_eq = [&]( auto a, auto b )
    {
        if ( a.pointer() || b.pointer() )
        {
            GenericPointer pa( a.objid() ), pb( b.objid() );
            if ( !pa.heap() || !pb.heap() || ( heap().valid( pa ) && heap().valid( pb ) ) )
                return;
            if ( a.objid() || b.objid() )
                fault( _VM_F_PtrCompare ) << "pointer-dependent equality comparison of "
                                          << a << " and " << b;
        }
    };

    auto _icmp_chk = [&]( auto a, auto b )
    {
        if ( a.pointer() && b.pointer() && a.objid() == b.objid() )
            return; /* ok */
        if ( a.pointer() || b.pointer() )
            fault( _VM_F_PtrCompare ) << "pointer-dependent inequality comparison of "
                                      << a << " and " << b;
    };

    auto _icmp_eq     = [&]( auto impl ) { _icmp_impl( _icmp_chk_eq, impl ); };
    auto _icmp        = [&]( auto impl ) { _icmp_impl( _icmp_chk, impl ); };
    auto _icmp_signed = [&]( auto impl ) -> void
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

    auto trace = [&]( auto &stream )
    {
        stream << opname( instruction() );
        for ( int i = 1; i <= instruction().argcount(); ++i )
            if ( instruction().value( i ).aggregate() )
                stream << " <agg>";
            else
                op< Any >( i, [&]( auto v ) { stream << " " << v.get( i ); } );
    };

    TRACE( trace );

    /* instruction dispatch */

    switch ( instruction().opcode )
    {
        case OpCode::GetElementPtr:
            result( operand< PointerV >( 0 ) +
                    gep( instruction().subcode, 1, instruction().argcount() ) );
            return;

        case OpCode::Select:
        {
            auto select = operand< BoolV >( 0 );

            slot_copy( s2ptr( operand( select.cooked() ? 1 : 2 ) ),
                        result(), result().size() );

            if ( !(select.defbits() & 1) )
                fault( _VM_F_Control ) << "select on an undefined value";
            /* TODO make the result undefined if !select.defined()? */
            return;
        }

        case OpCode::ICmp:
        {
            switch ( instruction().subcode )
            {
                case ICmpInst::ICMP_EQ:
                    return _icmp_eq( []( auto a, auto b ) { return a == b; } );
                case ICmpInst::ICMP_NE:
                    return _icmp_eq( []( auto a, auto b ) { return a != b; } );
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
                default: UNREACHABLE( "unexpected icmp op", instruction().subcode );
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
                    UNREACHABLE( "unexpected fcmp op", instruction().subcode );
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

                    T oldval( expected );
                    heap().read( ptr2h( ptr ), oldval );
                    auto change = oldval == expected;

                    if ( change.cooked() ) {
                        if ( !change.defined() )
                            newval.defined( false );
                        heap().write( ptr2h( ptr ), newval );
                    }
                    slot_write( result(), oldval, 0 );
                    slot_write( result(), change, sizeof( typename T::Raw ) );
                    if ( !change.defined() )
                        fault( _VM_F_Control )
                            << "atomic compare exchange depends on an undefined value"
                            << (oldval.defined() ? "" : " (old value not defined)")
                            << (newval.defined() ? "" : " (new value not defined)");
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
                    UNREACHABLE( "bad binop in atomicrmw" );
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
            UNREACHABLE( "unknown opcode", instruction().opcode );
    }
}

template< typename Ctx >
void Eval< Ctx >::run()
{
    context().reset_interrupted();
    do {
        advance();
        dispatch();
    } while ( !context().flags_any( _VM_CF_Stop ) );
}

template< typename Ctx >
bool Eval< Ctx >::run_seq( bool continued )
{
    if ( continued )
        refresh(), dispatch();
    else
        context().reset_interrupted();

    do {
        advance();
        if ( instruction().opcode == lx::OpHypercall && instruction().subcode == lx::HypercallChoose )
            return true;
        dispatch();
    } while ( !context().flags_any( _VM_CF_Stop ) );

    return false;
}

}

// vim: syntax=cpp tabstop=4 shiftwidth=4 expandtab ft=cpp
