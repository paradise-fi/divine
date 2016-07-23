// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2012-2016 Petr Ročkai <code@fixp.eu>
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

#include <divine/vm/program.hpp>
#include <divine/vm/value.hpp>
#include <divine/vm/heap.hpp> /* for tests */

#include <runtime/divine.h>

#include <brick-data>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/GetElementPtrTypeIterator.h>
#include <llvm/IR/CallSite.h>
#include <llvm/IR/DataLayout.h>
DIVINE_UNRELAX_WARNINGS

#include <algorithm>
#include <cmath>
#include <type_traits>
#include <unordered_set>

namespace llvm {
template<typename T> class generic_gep_type_iterator;
typedef generic_gep_type_iterator<User::const_op_iterator> gep_type_iterator;
}

namespace divine {
namespace vm {

using Fault = ::_VM_Fault;

using ::llvm::dyn_cast;
using ::llvm::cast;
using ::llvm::isa;
using ::llvm::ICmpInst;
using ::llvm::FCmpInst;
using ::llvm::AtomicRMWInst;
namespace Intrinsic = ::llvm::Intrinsic;
using ::llvm::CallSite;
using ::llvm::Type;
using brick::types::Unit;

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

/*
 * An efficient evaluator for the LLVM instruction set. Current semantics
 * (mainly) for arithmetic are derived from C++ semantics, which may not be
 * 100% correct. There are two main APIs: dispatch() and run(), the latter of
 * which executes until the 'ret' instruction in the topmost frame is executed,
 * at which point it gives back the return value passed to that 'ret'
 * instruction. The return value type must match that of the 'Result' template
 * parameter.
 */
template < typename Program, typename Context, typename Result >
struct Eval
{
    Program &_program;
    Context &_context;

    using Slot = typename Program::Slot;
    using Instruction = typename Program::Instruction;
    using HeapPointer = typename Context::Heap::Pointer;

    auto &heap() { return _context.heap(); }
    auto &control() { return _context.control(); }
    auto &program() { return _program; }
    auto &layout() { return program().TD; }

    Eval( Program &p, Context &c )
        : _program( p ), _context( c )
    {}

    using OpCode = llvm::Instruction;
    Instruction *_instruction;
    Instruction &instruction() { return *_instruction; }
    Result _result;
    std::unordered_set< GenericPointer > _cfl_visited;
    bool _interrupted;

    using PointerV = value::Pointer< HeapPointer >;
    using BoolV = value::Bool;
    /* TODO should be sizeof( int ) of the *bitcode*, not ours! */
    using IntV = value::Int< 8 * sizeof( int ), true >;
    using CharV = value::Int< 1, false >;

    static_assert( Convertible< PointerV >::template Guard< IntV >::value, "" );
    static_assert( Convertible< IntV >::template Guard< PointerV >::value, "" );
    static_assert( Convertible< value::Int< 64, true > >::template Guard< PointerV >::value, "" );
    static_assert( IntegerComparable< IntV >::value, "" );

    CodePointer pc()
    {
        auto ptr = heap().template read< PointerV >( frame().cooked() );
        if ( !ptr.defined() )
            return CodePointer();
        return ptr.cooked();
    }

    void pc( CodePointer p ) { heap().write( frame().cooked(), PointerV( p ) ); }

    PointerV frame() { return control().frame(); }
    PointerV globals() { return control().globals(); }
    PointerV constants() { return control().constants(); }

    GenericPointer s2ptr( Slot v, int off, PointerV f, PointerV g )
    {
        auto base = PointerV();
        switch ( v.location )
        {
            case Slot::Local: base = f + PointerBytes * 2; break;
            case Slot::Global: base = g; break;
            case Slot::Constant: base = constants(); break;
            default: UNREACHABLE( "impossible slot type" );
        }
        ASSERT( base.defined() );
        // std::cerr << "s2ptr: " << v << " -> " << (base + v.offset + off) << std::endl;
        return (base + v.offset + off).cooked();
    }
    GenericPointer s2ptr( Slot v, int off, PointerV f ) { return s2ptr( v, off, f, globals() ); }
    GenericPointer s2ptr( Slot v, int off = 0 ) { return s2ptr( v, off, frame(), globals() ); }

    HeapPointer ptr2h( PointerV p ) { return ptr2h( p, globals() ); }
    HeapPointer ptr2h( PointerV p, PointerV g )
    {
        ASSERT( p.defined() );
        if ( p.cooked().type() == PointerType::Heap )
            return p.cooked();
        if ( p.cooked().type() == PointerType::Const )
        {
            ConstPointer pp = p.cooked();
            return s2ptr( program()._constants[ pp.object() ], pp.offset() );
        }
        if ( p.cooked().type() == PointerType::Global )
        {
            GlobalPointer pp = p.cooked();
            return s2ptr( program()._globals[ pp.object() ], pp.offset(), nullPointer(), g );
        }
        UNREACHABLE_F( "a bad pointer in ptr2h: %s", brick::string::fmt( PointerV( p ) ).c_str() );
    }

    struct FaultStream : std::stringstream
    {
        using Control = typename Context::Control;
        Control *_ctrl;
        Fault _fault;
        bool _trace;

        FaultStream( Control &c, Fault f, bool t ) : _ctrl( &c ), _fault( f ), _trace( t ) {}
        FaultStream( FaultStream &fs ) : _ctrl( fs._ctrl ), _fault( fs._fault ), _trace( fs._trace )
        {
            fs._ctrl = nullptr;
        }

        ~FaultStream()
        {
            if ( !_ctrl )
                return;
            if ( _trace )
                _ctrl->trace( "FAULT: " + str() );
            _ctrl->fault( _fault );
        }
    };

    FaultStream fault( Fault f )
    {
        FaultStream fs( control(), f, true );
        return fs;
    }

    template< typename _T >
    struct V
    {
        using T = _T;
        Eval *ev;
        V( Eval *ev ) : ev( ev ) {}

        GenericPointer ptr( int v ) { return ev->s2ptr( ev->instruction().value( v ) ); }
        T get( int v  = INT_MIN )
        {
            ASSERT_LEQ( INT_MIN + 1, v ); /* default INT_MIN is only for use in decltype */
            return ev->heap().template read< T >( ptr( v ) );
        }
        void set( int v, T t ) { ev->heap().write( ptr( v ), t ); }
    };

    template< typename T > T operand( int i ) { return V< T >( this ).get( i >= 0 ? i + 1 : i ); }
    template< typename T > void result( T t ) { V< T >( this ).set( 0, t ); }
    auto operand( int i ) { return instruction().operand( i ); }
    auto result() { return instruction().result(); }

    template< typename T > auto operandCk( int i )
    {
        auto op = operand< T >( i );
        if ( !op.defined() )
            fault( _VM_F_Hypercall ) << "operand " << i << " has undefined value";
        return op;
    }

    PointerV operandPtr( int i )
    {
        auto op = operand< PointerV >( i );
        if ( !op.defined() )
            fault( _VM_F_Hypercall ) << "pointer operand " << i << " has undefined value";
        return op;
    }

    using AtOffset = std::pair< IntV, Type * >;

    AtOffset compositeOffset( Type *t ) {
        return std::make_pair( IntV( 0 ), t );
    }

    template< typename... Args >
    AtOffset compositeOffset( Type *t, IntV index, Args... indices )
    {
        IntV offset( 0 );

        if (::llvm::StructType *STy = dyn_cast< ::llvm::StructType >( t )) {
            const ::llvm::StructLayout *SLO = layout().getStructLayout(STy);
            offset = IntV( SLO->getElementOffset( index.cooked() ) );
        } else {
            const ::llvm::SequentialType *ST = cast< ::llvm::SequentialType >( t );
            offset = IntV( index.cooked() * layout().getTypeAllocSize( ST->getElementType() ) );
        }

        offset.defined( index.defined() );
        auto r = compositeOffset(
            cast< ::llvm::CompositeType >( t )->getTypeAtIndex( index.cooked() ), indices... );
        return std::make_pair( r.first + offset, r.second );
    }

    IntV compositeOffsetFromInsn( Type *t, int current, int end )
    {
        if ( current == end )
            return IntV( 0 );

        auto index = operand< IntV >( current );
        auto r = compositeOffset( t, index );

        return r.first + compositeOffsetFromInsn( r.second, current + 1, end );
    }

    void implement_store()
    {
        auto to = operandCk< PointerV >( 1 );
        /* std::cerr << "store of *" << s2ptr( operand( 0 ) ) << " to "
           << operand< PointerV >( 1 ) << std::endl; */
        if ( !heap().copy( s2ptr( operand( 0 ) ), ptr2h( to ), operand( 0 ).width ) )
            fault( _VM_F_Memory ) << "invalid store to " << to;
    }

    void implement_load()
    {
        auto from = operandPtr( 0 );
        if ( !from.defined() )
            return; // handled in operandPtr
        if ( !heap().copy( ptr2h( from ), s2ptr( result() ), result().width ) )
            fault( _VM_F_Memory ) << "invalid load from " << from;
    }

    template< typename > struct Any { static const bool value = true; };

    template< template< typename > class Guard, typename T, typename Op >
    auto op( Op _op ) -> typename std::enable_if< Guard< T >::value >::type
    {
        // std::cerr << "op called on type " << typeid( T ).name() << std::endl;
        // std::cerr << instruction() << std::endl;
        _op( V< T >( this ) );
    }

    template< template< typename > class Guard, typename T >
    void op( NoOp )
    {
        instruction().op->dump();
        UNREACHABLE_F( "invalid operation on %s", typeid( T ).name() );
    }

    template< template< typename > class Guard, typename Op >
    void op( int off, Op _op )
    {
        auto &v = instruction().value( off );
        switch ( v.type ) {
            case Slot::Integer:
                switch ( v.width )
                {
                    case 1: return op< Guard, value::Int<  8 > >( _op );
                    case 2: return op< Guard, value::Int< 16 > >( _op );
                    case 4: return op< Guard, value::Int< 32 > >( _op );
                    case 8: return op< Guard, value::Int< 64 > >( _op );
                }
                UNREACHABLE_F( "Unsupported integer width %d", v.width );
            case Slot::Pointer: case Slot::Alloca: case Slot::CodePointer:
                return op< Guard, PointerV >( _op );
            case Slot::Float:
                switch ( v.width )
                {
                    case sizeof( float ):
                        return op< Guard, value::Float< float > >( _op );
                    case sizeof( double ):
                        return op< Guard, value::Float< double > >( _op );
                    case sizeof( long double ):
                        return op< Guard, value::Float< long double > >( _op );
                }
            case Slot::Void:
                return;
            default:
                UNREACHABLE_F( "an unexpected dispatch type %d", v.type );
        }
    }

    template< template< typename > class Guard, typename Op >
    void op( int off1, int off2, Op _op )
    {
        op< Any >( off1, [&]( auto v1 ) {
                return this->op< Guard< decltype( v1.get() ) >::template Guard >(
                    off2, [&]( auto v2 ) { return _op( v1, v2 ); } );
            } );
    }

    void implement_alloca()
    {
        ::llvm::AllocaInst *I = cast< ::llvm::AllocaInst >( instruction().op );
        Type *ty = I->getAllocatedType();

        int count = operandCk< IntV >( 0 ).cooked();
        int size = layout().getTypeAllocSize(ty); /* possibly aggregate */

        unsigned alloc = std::max( 1, count * size );
        auto res = PointerV( heap().make( alloc ) );
        result( res );
        // std::cerr << "alloca'd " << res << std::endl;
    }

    void implement_extractvalue()
    {
        auto off = compositeOffsetFromInsn( instruction().op->getOperand(0)->getType(), 1,
                                            instruction().values.size() - 1 );
        ASSERT( off.defined() );
        heap().copy( s2ptr( operand( 0 ), off.cooked() ), s2ptr( result() ), result().width );
    }

    void implement_insertvalue()
    {
        /* first copy the original */
        heap().copy( s2ptr( operand( 0 ) ), s2ptr( result() ), result().width );
        auto off = compositeOffsetFromInsn( instruction().op->getOperand(0)->getType(), 2,
                                            instruction().values.size() - 1 );
        ASSERT( off.defined() );
        /* write the new value over the selected field */
        heap().copy( s2ptr( operand( 1 ) ), s2ptr( result(), off.cooked() ), operand( 1 ).width );
    }

    void jumpTo( PointerV _to )
    {
        CodePointer to( _to.cooked() );
        if ( pc().function() != to.function() )
            UNREACHABLE( "illegal cross-function jump" );
        switchBB( to );
    }

    void implement_ret()
    {
        auto fr = frame();
        heap().skip( fr, sizeof( typename PointerV::Raw ) );
        auto parent = heap().template read< PointerV >( fr.cooked() );

        if ( !heap().valid( parent.cooked() ) )
        {
            if ( control().isEntryFrame( frame().cooked() ) )
            {
                if ( instruction().values.size() > 1 )
                    _result = operand< Result >( 0 );
            }
            else
            {
                control().mask( false );
                set_interrupted( true );
            }
            control().frame( nullPointer() );
            return;
        }

        auto caller_pc = heap().template read< PointerV >( parent.cooked() );
        auto caller = _program.instruction( caller_pc.cooked() );
        if ( instruction().values.size() > 1 ) { /* return value */
            if ( caller.values.size() == 0 )
                fault( _VM_F_Control ) << "Function which was called as void returned a value";
            else if ( caller.result().width < operand( 0 ).width )
                fault( _VM_F_Control ) << "Returned value is bigger then expected by caller";
            else if ( !heap().copy( s2ptr( operand( 0 ) ),
                             s2ptr( caller.result(), 0, parent.cooked() ),
                             caller.result().width ) )
                fault( _VM_F_Memory ) << "Cound not return value";
        }

        control().frame( parent.cooked() );

        if ( isa< ::llvm::InvokeInst >( caller.op ) )
        {
            auto rv = s2ptr( caller.operand( -2 ), 0, parent.cooked() );
            auto br = heap().template read< PointerV >( rv );
            jumpTo( br );
        }
    }

    void implement_br()
    {
        if ( instruction().values.size() == 2 )
            jumpTo( operandCk< PointerV >( 0 ) );
        else
        {
            auto cond = operand< BoolV >( 0 );
            if ( !cond.defined() )
                fault( _VM_F_Control );
            if ( cond.cooked() )
                jumpTo( operandCk< PointerV >( 2 ) );
            else
                jumpTo( operandCk< PointerV >( 1 ) );
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
            auto &i = _program.instruction( pc );
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
        pc( target );
        // std::cerr << "switchBB: " << pc() << std::endl;

        target.instruction( target.instruction() + 1 );
        auto &i0 = _program.instruction( target );
        if ( i0.opcode != OpCode::PHI )
            return;

        int size = 0, offset = 0, count = 0;
        auto BB = cast< llvm::Instruction >( _program.instruction( origin ).op )->getParent();
        int idx = cast< llvm::PHINode >( i0.op )->getBasicBlockIndex( BB );

        each_phi( target, [&]( auto &i )
                  {
                      ASSERT_EQ( cast< llvm::PHINode >( i.op )->getBasicBlockIndex( BB ), idx );
                      size += i.result().width;
                      ++ count;
                  } );
        auto tmp = heap().make( size ), tgt = tmp;
        each_phi( target, [&]( auto &i )
                  {
                      heap().copy( s2ptr( i.operand( idx ) ), tgt.cooked(), i.result().width );
                      heap().skip( tgt, i.result().width );
                  } );
        tgt = tmp;
        each_phi( target, [&]( auto &i )
                  {
                      heap().copy( tgt.cooked(), s2ptr( i.result() ), i.result().width );
                      heap().skip( tgt, i.result().width );
                  } );

        heap().free( tmp.cooked() );
        target.instruction( target.instruction() + count - 1 );
        pc( target );
    }

    template< typename O >
    void collect_allocas( O o )
    {
        auto &f = program().functions[ pc().function() ];
        for ( auto &i : f.instructions )
            if ( i.opcode == OpCode::Alloca )
            {
                auto ptr = heap().template read< PointerV >( s2ptr( i.result() ) );
                if ( heap().valid( ptr.cooked() ) )
                    *o++ = ptr;
            }
    }

    void implement_stacksave()
    {
        std::vector< PointerV > ptrs;
        collect_allocas( std::back_inserter( ptrs ) );
        auto r = heap().make( sizeof( IntV::Raw ) + ptrs.size() * PointerBytes );
        auto p = r;
        heap().shift( p, IntV( ptrs.size() ) );
        for ( auto ptr : ptrs )
            heap().shift( p, ptr );
        result( r );
    }

    void implement_stackrestore()
    {
        std::vector< PointerV > ptrs;
        collect_allocas( std::back_inserter( ptrs ) );
        auto r = operandPtr( 0 );
        auto count = heap().template shift< IntV >( r );
        if ( !count.defined() )
            fault( _VM_F_Hypercall );
        auto s = r;

        for ( auto ptr : ptrs )
        {
            bool retain = false;
            r = s;
            for ( int i = 0; i < count.cooked(); ++i )
            {
                auto p = heap().template shift< PointerV >( r );
                auto eq = ptr == p;
                if ( !eq.defined() )
                {
                    fault( _VM_F_Hypercall );
                    break;
                }
                if ( eq.cooked() )
                {
                    retain = true;
                    break;
                }
            }
            if ( !retain )
                heap().free( ptr.cooked() );
        }
    }

    void implement_intrinsic( int id ) {
        switch ( id ) {
            case Intrinsic::vastart:
            {
                // writes a pointer to a monolithic block of memory that
                // contains all the varargs, successively assigned higher
                // addresses (going from left to right in the argument list) to
                // the argument of the intrinsic
                auto f = _program.functions[ pc().function() ];
                if ( !f.vararg ) {
                    fault( _VM_F_Hypercall ) << "va_start called in non-variadic function";
                    return;
                }
                auto vaptr_loc = s2ptr( f.values[ f.argcount ] );
                auto vaListPtr = operandPtr( 0 );
                if ( !vaListPtr.defined() )
                    return;
                if ( !heap().copy( vaptr_loc, ptr2h( vaListPtr ), operand( 0 ).width ) )
                    fault( _VM_F_Memory ) << "invalid va_start with va_list = " << vaListPtr;
                return;
            }
            case Intrinsic::vaend: return;
            case Intrinsic::vacopy:
            {
                auto from = operandPtr( 1 );
                auto to = operandPtr( 0 );
                // note: we are writting pointer, so with of from/to is also with of operand
                if ( !heap().copy( ptr2h( from ), ptr2h( to ), operand( 0 ).width ) )
                    fault( _VM_F_Memory ) << "invalid va_copy from " << from << " to " << to;
                return;
            }
            case Intrinsic::trap:
                NOT_IMPLEMENTED(); /* TODO */
            case Intrinsic::eh_typeid_for:
                result( IntV( program().function( pc() ).typeID(
                                  operandCk< PointerV >( 0 ).cooked() ) ) );
                return;
            case Intrinsic::smul_with_overflow:
            case Intrinsic::sadd_with_overflow:
            case Intrinsic::ssub_with_overflow:
            case Intrinsic::umul_with_overflow:
            case Intrinsic::uadd_with_overflow:
            case Intrinsic::usub_with_overflow:
                NOT_IMPLEMENTED();
                return;
            case Intrinsic::stacksave:
                return implement_stacksave();
            case Intrinsic::stackrestore:
                return implement_stackrestore();
            default:
                /* We lowered everything else in buildInfo. */
                instruction().op->dump();
                UNREACHABLE_F( "unexpected intrinsic %d", id );
        }
    }

    template< typename V >
    void check( V v ) { if ( !v.defined() ) fault( _VM_F_Hypercall ); }

    std::string operandStr( int o )
    {
        PointerV nptr = ptr2h( operandCk< PointerV >( o ) );
        std::string str;
        CharV c;
        do {
            c = heap().template shift< CharV >( nptr );
            if ( c.cooked() )
                str += c.cooked();
        } while ( c.cooked() );
        return str;
    }

    bool set_interrupted( bool i )
    {
        bool rv = _interrupted;
        _cfl_visited.clear();
        _interrupted = i;
        return rv;
    }

    void implement_hypercall()
    {
        switch( instruction().hypercall )
        {
            case HypercallChoose:
            {
                int options = operandCk< IntV >( 0 ).cooked();
                std::vector< int > p;
                for ( int i = 1; i < int( instruction().values.size() ) - 2; ++i )
                    p.push_back( operandCk< IntV >( i ).cooked() );
                if ( !p.empty() && int( p.size() ) != options )
                    fault( _VM_F_Hypercall );
                else
                    result( IntV( control().choose( options, p.begin(), p.end() ) ) );
                return;
            }
            case HypercallSetSched:
                return control().setSched( operandCk< PointerV >( 0 ).cooked() );
            case HypercallSetFault:
                return control().setFault( operandCk< PointerV >( 0 ).cooked() );
            case HypercallSetIfl:
                return control().setIfl( operandCk< PointerV >( 0 ) );
            case HypercallInterrupt:
                result( IntV( set_interrupted( operandCk< IntV >( 0 ).cooked() ) ) );
            case HypercallCflInterrupt:
                if ( _cfl_visited.count( pc() ) )
                    set_interrupted( true );
                else
                    _cfl_visited.insert( pc() );
                return;
            case HypercallMemInterrupt:
                set_interrupted( true ); return; /* TODO */
            case HypercallJump:
            {
                // std::cerr << "======= jump" << std::endl;
                auto tgt = operandCk< PointerV >( 0 );
                auto forget = operandCk< IntV >( 1 ).cooked();
                if ( forget )
                {
                    control().mask( false );
                    set_interrupted( false );
                }
                if ( tgt.cooked() == nullPointer() )
                    fault( _VM_F_Hypercall ) << "target frame of a jump is null";
                else
                    control().frame( tgt );
                return;
            }
            case HypercallTrace:
            {
                auto str = operandStr( 0 );
                control().trace( str );
                return;
            }
            case HypercallFault:
                fault( Fault( operandCk< IntV >( 0 ).cooked() ) ) << "__vm_fault called";
                return;
            case HypercallMask:
                result( IntV( control().mask( operandCk< IntV >( 0 ).cooked() ) ) );
                return;
            case HypercallMakeObject:
            {
                int64_t size = operandCk< IntV >( 0 ).cooked();
                if ( size >= ( 2ll << PointerOffBits ) || size < 1 )
                {
                    fault( _VM_F_Hypercall ) << "invalid size " << size << " passed to __vm_make_object";
                    size = 0;
                }
                result( size ? heap().make( size ) : PointerV( nullPointer() ) );
                return;
            }
            case HypercallFreeObject:
                if ( !heap().free( operand< PointerV >( 0 ).cooked() ) )
                    fault( _VM_F_Memory ) << "invalid pointer passed to __vm_free_object";
                return;
            case HypercallQueryObjectSize:
            {
                auto ptr = operandCk< PointerV >( 0 ).cooked();
                if ( !heap().valid( ptr ) )
                    fault( _VM_F_Hypercall ) << "invalid pointer " << ptr
                                             << " passed to __vm_query_object_size";
                else
                    result( IntV( heap().size( ptr ) ) );
                return;
            }
            default:
                UNREACHABLE_F( "unknown hypercall %d", instruction().hypercall );
        }
    }

    void implement_call( bool invoke )
    {
        if ( instruction().hypercall )
        {
            if ( invoke )
                fault( _VM_F_Control ) << "illegal 'invoke' of a hypercall";
            else
                return implement_hypercall();
            return;
        }

        CodePointer target = operandCk< PointerV >( invoke ? -3 : -1 ).cooked();
        CallSite CS( cast< ::llvm::Instruction >( instruction().op ) );

        if ( !target.function() )
        {
            ::llvm::Function *F = CS.getCalledFunction();

            if ( F && F->isDeclaration() )
            {
                auto id = F->getIntrinsicID();
                if ( id != Intrinsic::not_intrinsic )
                    return implement_intrinsic( id );
            }
            else
            {
                fault( _VM_F_Control ) << "invalid call on a null pointer";
                return;
            }
        }

        const auto &function = _program.function( target );

        /* report problems with the call before pushing the new stackframe */
        if ( !function.vararg && int( CS.arg_size() ) > function.argcount )
        {
            fault( _VM_F_Control ) << "too many arguments given to a call: "
                                   << function.argcount << " expected but " << CS.arg_size() << " given";
            return;
        }

        auto frameptr = heap().make( program().function( target ).datasize + 2 * PointerBytes );
        auto p = frameptr;
        heap().shift( p, PointerV( target ) );
        heap().shift( p, PointerV( frame() ) );

        /* Copy arguments to the new frame. */
        for ( int i = 0; i < int( CS.arg_size() ) && i < int( function.argcount ); ++i )
            heap().copy( s2ptr( operand( i ) ),
                         s2ptr( function.values[ i ], 0, frameptr.cooked() ),
                         function.values[ i ].width );

        if ( function.vararg )
        {
            int size = 0;
            for ( int i = function.argcount; i < int( CS.arg_size() ); ++i )
                size += operand( i ).width;
            auto vaptr = size ? heap().make( size ) : PointerV( nullPointer() );
            auto vaptr_loc = s2ptr( function.values[ function.argcount ], 0, frameptr.cooked() );
            heap().write( vaptr_loc, vaptr );
            for ( int i = function.argcount; i < int( CS.arg_size() ); ++i )
            {
                auto op = operand( i );
                heap().copy( s2ptr( op ), vaptr.cooked(), op.width );
                heap().skip( vaptr, int( op.width ) );
            }
        }

        ASSERT( !isa< ::llvm::PHINode >( instruction().op ) );
        control().frame( frameptr.cooked() );
    }

    void run()
    {
        _interrupted = false;
        CodePointer next = pc();
        do {
            pc( next );
            // std::cerr << "pc = " << pc() << std::endl;
            _instruction = &program().instruction( pc() );
            // _instruction->op->dump();
            dispatch();
            // op< Any >( 0, []( auto v ) { std::cerr << "  result = " << v.get( 0 ) << std::endl; } );
            if ( _interrupted && !control().mask() )
            {
                // std::cerr << "======= interrupt" << std::endl;
                control().interrupt();
                _interrupted = false;
            }
            if ( control().frame().cooked() != nullPointer() )
            {
                next = pc();
                next.instruction( next.instruction() + 1 );
            }
        } while ( control().frame().cooked() != nullPointer() );
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
                        this->fault( _VM_F_Arithmetic ) << "division by zero or an undefined number";
                        result( decltype( v.get() )( 0 ) );
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
                    using T = decltype( v.get() );
                    auto loc = operandPtr( 0 );
                    if ( !loc.defined() )
                        return;
                    HeapPointer hloc = ptr2h( loc );
                    if ( !heap().valid( hloc ) ) {
                        this->fault( _VM_F_Memory ) << "invalid AtomicRMW at " << loc;
                        return; // TODO: destory pre-existing register value
                    }
                    auto edit = heap().template read< T >( hloc );
                    this->result( edit );
                    heap().write( hloc, impl( edit, v.get( 2 ) ) );
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
                            instruction().op->getOperand(0)->getType(), 1,
                            instruction().values.size() - 1 ) );
                return;

            case OpCode::Select:
            {
                auto select = operand< BoolV >( 0 );

                if ( !select.defined() )
                    fault( _VM_F_Control ) << "select on an undefined value";

                heap().copy( s2ptr( operand( select.cooked() ? 1 : 2 ) ),
                             s2ptr( result() ), result().width );
                /* TODO make the result undefined if !select.defined()? */
                return;
            }

            case OpCode::ICmp:
            {
                auto p = cast< ICmpInst >( instruction().op )->getPredicate();
                switch ( p )
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
                    default: UNREACHABLE_F( "unexpected icmp op %d", p );
                }
            }


            case OpCode::FCmp:
            {
                auto p = cast< FCmpInst >( instruction().op )->getPredicate();
                bool nan, defined;

                this->op< IsFloat >( 1, [&]( auto v )
                       {
                           nan = v.get( 1 ).isnan() || v.get( 2 ).isnan();
                           defined = v.get( 1 ).defined() && v.get( 2 ).defined();
                       } );

                switch ( p )
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
                    switch ( p )
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

                switch ( p )
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
                        UNREACHABLE_F( "unexpected fcmp op %d", p );
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
                        if ( !v.get( 1 ).defined() )
                            fault( _VM_F_Control ) << "switch on an undefined value";
                        for ( int o = 2; o < int( this->instruction().values.size() ) - 1; o += 2 ) {
                            auto eq = v.get( 1 ) == v.get( o + 1 );
                            if ( !eq.defined() )
                                fault( _VM_F_Control )
                                    << "comparison result undefined for a switch branch";
                            if ( eq.cooked() )
                                return this->jumpTo( operandCk< PointerV >( o + 1 ) );
                        }
                        return this->jumpTo( operandCk< PointerV >( 1 ) );
                    } );

            case OpCode::Call:
                implement_call( false ); break;
            case OpCode::Invoke:
                implement_call( true ); break;
            case OpCode::Ret:
                implement_ret(); break;

            case OpCode::BitCast:
                heap().copy( s2ptr( operand( 0 ) ), s2ptr( result() ), result().width );
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
                        auto ptr = operandPtr( 0 );
                        auto expected = v.get( 2 );
                        auto newval = v.get( 3 );

                        if ( !heap().valid( ptr.cooked() ) ) {
                            this->fault( _VM_F_Memory ) << "invalid pointer in cmpxchg" << ptr;
                            return;
                        }
                        auto oldval = heap().template read< T >( ptr.cooked() );
                        auto change = oldval == expected;
                        auto resptr = s2ptr( result() );

                        if ( change.cooked() ) {
                            if ( !change.defined() || !ptr.defined() ) // undefine if one of the inputs was not defined
                                newval.defined( false );
                            heap().write( ptr.cooked(), newval );
                        }
                        heap().write( resptr, oldval );
                        resptr.offset( resptr.offset() + sizeof( typename T::Raw ) );
                        heap().write( resptr, change );
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
                switch ( cast< AtomicRMWInst >( instruction().op )->getOperation() )
                {
                    case AtomicRMWInst::Xchg:
                        return _atomicrmw( []( auto v, auto x ) { return x; } );
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
                    // note: although the va_list type might not be a pointer (as on x86)
                    // we will use it so, assuming that it will be at least as big as a
                    // pointer (for example, on x86_64 va_list is { i32, i32, i64*, i64* }*)
                    auto vaListRaw = operandPtr( 0 );
                    if ( !vaListRaw.defined() )
                        return;
                    auto vaListPtr = ptr2h( vaListRaw );
                    if ( !heap().valid( vaListPtr ) )
                        fault( _VM_F_Memory ) << "invalid va_list " << vaListRaw;
                    auto vaArgsRaw = heap().template read< PointerV >( vaListPtr );
                    if ( !vaArgsRaw.defined() ) {
                        fault( _VM_F_Hypercall ) << "undefined va_list value " << vaArgsRaw;
                        return;
                    }
                    auto vaArgs = ptr2h( vaArgsRaw );
                    if ( !heap().valid( vaArgs ) ) {
                        fault( _VM_F_Memory ) << "invalid va_list arg " << vaArgs;
                        return;
                    }
                    if ( !heap().copy( ptr2h( vaArgs ), s2ptr( result() ), result().width ) )
                        fault( _VM_F_Memory ) << "invalid load of va_arg from " << vaArgs;
                    heap().template write< PointerV >( vaListPtr, vaArgs + result().width );
                }
                return;

            case OpCode::LandingPad:
                break; /* nothing to do, handled by the unwinder */

            case OpCode::Fence: /* noop until we have reordering simulation */
                break;

            default:
                instruction().op->dump();
                UNREACHABLE_F( "unknown opcode %d", instruction().opcode );
        }
    }
};


}

namespace t_vm
{

using vm::CodePointer;

struct TProgram
{
    struct Slot {};
    struct Instruction {};
};

template< typename Prog >
struct TContext
{
    using Heap = vm::MutableHeap;
    using HeapPointer = Heap::Pointer;
    using PointerV = vm::value::Pointer< HeapPointer >;
    using Control = TContext< Prog >; /* Self */

    vm::Fault _fault;
    PointerV _constants, _globals, _frame, _entry_frame;
    Prog &_program;
    Heap _heap;

    PointerV globals() { return _globals; }
    PointerV constants() { return _constants; }
    bool mask( bool = false )  { return false; }
    void interrupt() {}

    template< typename I >
    int choose( int, I, I ) { return 0; }

    PointerV frame() { return _frame; }
    void frame( PointerV p ) { _frame = p; }
    bool isEntryFrame( HeapPointer fr ) { return HeapPointer( _entry_frame.cooked() ) == fr; }

    void fault( vm::Fault f ) { _fault = f; _frame = vm::nullPointer(); }
    void doublefault() { UNREACHABLE( "double fault" ); }
    void trace( std::string s ) { std::cerr << "T: " << s << std::endl; }

    void push( PointerV ) {}

    template< typename X, typename... Args >
    void push( PointerV p, X x, Args... args )
    {
        _heap.shift( p, x );
        push( p, args... );
    }

    template< typename... Args >
    void enter( CodePointer pc, PointerV parent, Args... args )
    {
        int datasz = _program.function( pc ).datasize;
        auto frameptr = _heap.make( datasz + 2 * vm::PointerBytes );
        _frame = frameptr;
        if ( parent.cooked() == vm::nullPointer() )
            _entry_frame = _frame;
        _heap.shift( frameptr, PointerV( pc ) );
        _heap.shift( frameptr, parent );
        push( frameptr, args... );
    }

    void setSched( CodePointer p ) {}
    void setFault( CodePointer p ) {}
    void setIfl( PointerV p ) {}

    Control &control() { return *this; }
    Heap &heap() { return _heap; }

    TContext( Prog &p ) : _fault( _VM_F_NoFault ), _program( p ) {}
};

struct Eval
{
    using IntV = vm::value::Int< 32 >;

    TEST(instance)
    {
        TProgram p;
        TContext< TProgram > c( p );
        vm::Eval< TProgram, TContext< TProgram >, vm::value::Void > e( p, c );
    }

    template< typename... Args >
    int testF( std::string s, Args... args )
    {
        auto p = compileC( s );
        TContext< vm::Program > c( *p );
        std::tie( c._constants, c._globals ) = p->exportHeap( c.heap() );
        vm::Eval< vm::Program, TContext< vm::Program >, IntV > e( *p, c );
        auto pc = p->functionByName( "f" );
        pc.instruction( 1 );
        c.control().enter( pc, vm::nullPointer(), args... );
        e.run();
        return e._result.cooked();
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
        int x = testF( "void *__vm_make_object( int ); int __vm_query_object_size( void * ); "
                       "int f() { void *x = __vm_make_object( 10 );"
                       "return __vm_query_object_size( x ); }" );
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
        const char *fdef = R"|(void __vm_trace( const char *v );
                               int r = 1;
                               void fail( const char *v ) { __vm_trace( v ); r = 0; }
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
};

}

}
