// -*- C++ -*- (c) 2012-2015 Petr Rockai
//             (c) 2015 Vladimír Štill

#include <divine/llvm/machine.h>
#include <divine/llvm/program.h>

#include <divine/toolkit/list.h>

#include <brick-data.h>

#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/GetElementPtrTypeIterator.h>
#include <llvm/IR/CallSite.h>
#include <llvm/IR/DataLayout.h>

#include <algorithm>
#include <cmath>
#include <type_traits>

#ifndef DIVINE_LLVM_EXECUTION_H
#define DIVINE_LLVM_EXECUTION_H

namespace llvm {
template<typename T> class generic_gep_type_iterator;
typedef generic_gep_type_iterator<User::const_op_iterator> gep_type_iterator;
}

namespace divine {
namespace llvm {

namespace {
 int8_t &make_signed(  uint8_t &x ) { return *reinterpret_cast<  int8_t * >( &x ); }
int16_t &make_signed( uint16_t &x ) { return *reinterpret_cast< int16_t * >( &x ); }
int32_t &make_signed( uint32_t &x ) { return *reinterpret_cast< int32_t * >( &x ); }
int64_t &make_signed( uint64_t &x ) { return *reinterpret_cast< int64_t * >( &x ); }

/* maybe assert out instead? */
float &make_signed( float &x ) { return x; }
double &make_signed( double &x ) { return x; }
long double &make_signed( long double &x ) { return x; }
}

using std::isnan;
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

template< typename T >
auto rem( T x, T y ) -> decltype( x % y ) { return x % y; }

template< typename T >
auto rem( T x, T y ) -> typename std::enable_if< std::is_floating_point< T >::value, T >::type
{ return std::fmod( x, y ); }

/* Dummy implementation of a ControlContext, useful for Evaluator for
 * control-flow-free snippets (like ConstantExpr). */
struct ControlContext {
    bool jumped;
    Choice choice;
    void enter( int ) { ASSERT_UNREACHABLE( "" ); }
    void leave() { ASSERT_UNREACHABLE( "" ); }
    machine::Frame &frame( int /*depth*/ = 0 ) { ASSERT_UNREACHABLE( "" ); }
    machine::Flags &flags() { ASSERT_UNREACHABLE( "" ); }
    void problem( Problem::What, Pointer = Pointer() ) { ASSERT_UNREACHABLE( "" ); }
    PC &pc() { ASSERT_UNREACHABLE( "" ); }
    int new_thread( PC, Maybe< Pointer >, MemoryFlag ) { ASSERT_UNREACHABLE( "" ); }
    int stackDepth() { ASSERT_UNREACHABLE( "" ); }
    int threadId() { ASSERT_UNREACHABLE( "" ); }
    int threadCount() { ASSERT_UNREACHABLE( "" ); }
    void switch_thread( int ) { ASSERT_UNREACHABLE( "" ); }
    void dump() {}
};

template< typename X >
struct Dummy {
    static X &v() { ASSERT_UNREACHABLE( "" ); }
};

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

template< typename To > struct SignedConvertible {
    template< typename From >
    struct Guard {
        static const bool value = std::is_arithmetic< To >::value &&
                                  std::is_arithmetic< From >::value &&
                                  Convertible< To >::template Guard< From >::value;
    };
};

template< typename T > struct IntegerComparable {
    static const bool value = std::is_integral< T >::value ||
                              std::is_same< T, Pointer >::value ||
                              std::is_same< T, PC >::value;
};

struct __X {};
static_assert( Convertible< double >::template Guard< float >::value, "" );
static_assert( !Convertible< __X >::template Guard< float >::value, "" );

/*
 * A relatively efficient evaluator for the LLVM instruction set. Current
 * semantics are derived from C++ semantics, which is not always entirely
 * correct. TBD.
 *
 * EvalContext provides access to the register file and memory. It needs to
 * provide at least TargetData, dereference( Value ), dereference( Pointer )
 * and malloc( int ).
 */
template < typename EvalContext, typename ControlContext >
struct Evaluator
{
    ProgramInfo &info;
    EvalContext &econtext;
    ControlContext &ccontext;

    Evaluator( ProgramInfo &i, EvalContext &e, ControlContext &c )
        : info( i ), econtext( e ), ccontext( c )
    {}

    bool is_signed;

    typedef ::llvm::Instruction LLVMInst;
    ProgramInfo::Instruction *_instruction;
    ProgramInfo::Instruction &instruction() { return *_instruction; }


    using ValueRefs = brick::data::SmallVector< ValueRef, 4 >;
    using MemoryFlags = brick::data::SmallVector< MemoryFlag, 4 >;

    ValueRefs values, result;
    brick::data::SmallVector< MemoryFlags, 4 > flags;

    template< typename X >
    char *dereference( X x ) { return econtext.dereference( x ); }

    template< typename... X >
    static Unit declcheck( X... ) { return Unit(); }

    std::vector< int > pointerId( bool nonempty = false ) {
        auto r = econtext.pointerId( cast< ::llvm::Instruction >( instruction().op ) );
        if ( nonempty && !r.size() )
            return std::vector< int >{ 0 };
        return r;
    }

    void checkPointsTo() { // TODO: check aa_use as well
        auto &r = instruction().result();

        if ( instruction().result().type == ProgramInfo::Value::Void )
            return;

        auto mflag = econtext.memoryflag( r );
        if ( !mflag.valid() || mflag.get() != MemoryFlag::HeapPointer )
            return; /* nothing to do */

        auto p = *reinterpret_cast< Pointer * >( econtext.dereference( r ) );
        if ( p.null() )
            return;

        int actual = econtext.pointerId( p );
        if ( !actual )
            return;

        auto expected = pointerId();
        for ( auto i: expected )
            if ( i == actual )
                return;

        ccontext.problem( Problem::PointsToViolated, p );
    }

    void implement_bitcast() {
        auto r = memcopy( instruction().operand( 0 ), instruction().result(),
						  instruction().result().width );
        ASSERT_EQ( r, Problem::NoProblem );
        static_cast< void >( r );
    }

    using AtOffset = std::pair< int, Type * >;

    AtOffset compositeOffset( Type *t ) {
        return std::make_pair( 0, t );
    }

    template< typename... Args >
    AtOffset compositeOffset( Type *t, int index, Args... indices )
    {
        int offset;

        if (::llvm::StructType *STy = dyn_cast< ::llvm::StructType >( t )) {
            const ::llvm::StructLayout *SLO = econtext.TD.getStructLayout(STy);
            offset = SLO->getElementOffset( index );
        } else {
            const ::llvm::SequentialType *ST = cast< ::llvm::SequentialType >( t );
            offset = index * econtext.TD.getTypeAllocSize( ST->getElementType() );
        }

        auto r = compositeOffset(
            cast< ::llvm::CompositeType >( t )->getTypeAtIndex( index ), indices... );
        return std::make_pair( r.first + offset, r.second );
    }

    int compositeOffsetFromInsn( Type *t, int current, int end )
    {
        if ( current == end )
            return 0;

        int index = _get< int >( instruction().operand( current ) );
        auto r = compositeOffset( t, index );

        return r.first + compositeOffsetFromInsn( r.second, current + 1, end );
    }

    void implement_store() {
        Pointer to = _get< Pointer >( instruction().operand( 1 ) );
        auto mf = econtext.memoryflag( instruction().operand( 1 ) );
        if ( mf.valid() )
            ASSERT( econtext.validate( to, mf.get() == MemoryFlag::HeapPointer ) );
        if ( auto r = memcopy( instruction().operand( 0 ), to, instruction().operand( 0 ).width ) )
            ccontext.problem( r, to );
    }

    void implement_load() {
        Pointer from = _get< Pointer >( instruction().operand( 0 ) );
        auto mf = econtext.memoryflag( instruction().operand( 0 ) );
        if ( mf.valid() )
            ASSERT( econtext.validate( from, mf.get() == MemoryFlag::HeapPointer ) );
        if ( auto r = memcopy( from, instruction().result(), instruction().result().width ) )
            ccontext.problem( r, from );
    }

    template < typename From, typename To, typename FromC, typename ToC >
    Problem::What memcopy( From f, To t, int bytes, FromC &fromc, ToC &toc )
    {
        return llvm::memcopy( f, t, bytes, fromc, toc );
    }

    template < typename From, typename To >
    Problem::What memcopy( From f, To t, int bytes ) {
        return memcopy( f, t, bytes, econtext, econtext );
    }

    template< typename T >
    struct s_get {
        Evaluator< EvalContext, ControlContext > *ev;
        s_get( Evaluator< EvalContext, ControlContext > *ev ) : ev( ev ) {}
        T &operator()( int i ) { return ev->_get< T >( i ); }
    };

    template< typename T >
    T &_get( int i ) { return *reinterpret_cast< T * >( dereference( instruction().value( i ) ) ); }

    template< typename T >
    T &_get( ValueRef r ) { return *reinterpret_cast< T * >( dereference( r ) ); }

    template< typename T >
    typename std::remove_reference< T >::type &_deref( int i ) {
        return *reinterpret_cast<
            typename std::remove_reference< T >::type * >(
                dereference( _get< Pointer >( i ) ) ); }

    template< typename > struct Any { static const bool value = true; };

    template< template< typename > class Guard, typename T, typename Op >
    auto op( Op _op ) -> typename std::enable_if< Guard<
        typename std::remove_reference< T >::type >::value >::type
    {
        _op( [this]( int i ) -> auto& { return this->_get< T >( i ); } );
    }

    template< template< typename > class Guard, typename T >
    void op( NoOp ) {
        instruction().op->dump();
        ASSERT_UNREACHABLE( "invalid operation" );
    }

    template< template< typename > class Guard, typename Op >
    void op( int off, Op _op ) {
        using Value = ProgramInfo::Value;
        auto &v = instruction().value( off );
        switch ( v.type ) {
            case Value::Integer:
                switch ( v.width ) {
                    case 1: return op< Guard,  uint8_t >( _op );
                    case 2: return op< Guard, uint16_t >( _op );
                    case 4: return op< Guard, uint32_t >( _op );
                    case 8: return op< Guard, uint64_t >( _op );
                }
                ASSERT_UNREACHABLE_F( "Unsupported integer width %d", v.width );
            case Value::Pointer: case Value::Alloca:
                return op< Guard, Pointer >( _op );
            case Value::CodePointer:
                return op< Guard, PC >( _op );
            case Value::Float:
                switch ( v.width ) {
                case sizeof(float):
                    return op< Guard, float >( _op );
                case sizeof(double):
                    return op< Guard, double >( _op );
                case sizeof(long double):
                    return op< Guard, long double >( _op );
            }
            default:
                ASSERT_UNREACHABLE_F( "Wrong dispatch type %d", v.type );
        }
    }

    template< template< typename > class Guard, typename Op >
    void op( int off1, int off2, Op _op ) {
        op< Any >( off1, [&]( auto get1 ) {
                return this->op< Guard< typename std::remove_reference< decltype( get1( 0 ) ) >::type
                                >::template Guard >(
                    off2, [&]( auto get2 ) { return _op( get1, get2 ); } );
            } );
    }

    void implement_alloca() {
        ::llvm::AllocaInst *I = cast< ::llvm::AllocaInst >( instruction().op );
        Type *ty = I->getAllocatedType();

        int count = _get< int >( instruction().operand( 0 ) );
        int size = econtext.TD.getTypeAllocSize(ty); /* possibly aggregate */

        unsigned alloc = std::max( 1, count * size );
        Pointer &p = *reinterpret_cast< Pointer * >(
            dereference( instruction().result() ) );
        p = econtext.malloc( alloc, pointerId( true )[0] );
        auto mflag = econtext.memoryflag( instruction().result() );
        mflag.set( MemoryFlag::HeapPointer ); ++ mflag;
        for ( int i = 1; i < instruction().result().width; ++i, ++mflag )
            mflag.set( MemoryFlag::Data );
    }

    void implement_extractvalue() {
        auto r = memcopy( ValueRef( instruction().operand( 0 ), 0, -1,
                                    compositeOffsetFromInsn( instruction().op->getOperand(0)->getType(), 1,
                                                             instruction().values.size() - 1 ) ),
                          instruction().result(), instruction().result().width );
        ASSERT_EQ( r, Problem::NoProblem );
        static_cast< void >( r );
    }

    void implement_insertvalue() { /* NB. Implemented against spec, UNTESTED! */
        /* first copy the original */
        auto r = memcopy( instruction().operand( 0 ), instruction().result(), instruction().result().width );
        ASSERT_EQ( r, Problem::NoProblem );
        /* write the new value over the selected field */
        r = memcopy( instruction().operand( 1 ),
                     ValueRef( instruction().result(), 0, -1,
                               compositeOffsetFromInsn( instruction().op->getOperand(0)->getType(), 2,
                                                        instruction().values.size() - 1 ) ),
                     instruction().operand( 1 ).width );
        ASSERT_EQ( r, Problem::NoProblem );
        static_cast< void >( r );
    }

    void jumpTo( ProgramInfo::Value v )
    {
        PC to = *reinterpret_cast< PC * >( dereference( v ) );
        jumpTo( to );
    }

    void jumpTo( PC to )
    {
        if ( ccontext.pc().function != to.function )
            ASSERT_UNREACHABLE_F( "Can't deal with cross-function jumps." );
        switchBB( to );
    }

    void maybe_die() {
        /* kill off everything if the main thread died */
        if ( ccontext.threadId() == 0 && ccontext.stackDepth() == 0 ) {
            for ( int i = 1; i < ccontext.threadCount(); ++i ) {
                ccontext.switch_thread( i );
                while ( ccontext.stackDepth() )
                    ccontext.leave();
            }
        }
    }

    void implement_ret() {
        if ( ccontext.stackDepth() == 1 ) {
            ccontext.leave();
            return maybe_die();
        }

        auto caller = info.instruction( ccontext.frame( 1 ).pc );
        if ( instruction().values.size() > 1 ) /* return value */
            memcopy( instruction().operand( 0 ), ValueRef( caller.result(), 1 ), caller.result().width );

        ccontext.leave();

        if ( isa< ::llvm::InvokeInst >( caller.op ) )
            jumpTo( caller.operand( -2 ) );
    }

    void implement_br()
    {
        if ( instruction().values.size() == 2 )
            jumpTo( instruction().operand( 0 ) );
        else {
            auto mflag = econtext.memoryflag( instruction().operand( 0 ) );
            if ( mflag.valid() && mflag.get() == MemoryFlag::Uninitialised )
                ccontext.problem( Problem::Uninitialised );
            if ( _get< bool >( instruction().operand( 0 ) ) )
                jumpTo( instruction().operand( 2 ) );
            else
                jumpTo( instruction().operand( 1 ) );
        }
    }

    void implement_indirectBr() {
        auto mflag = econtext.memoryflag( instruction().operand( 0 ) );
        if ( mflag.valid() && mflag.get() == MemoryFlag::Uninitialised )
            ccontext.problem( Problem::Uninitialised );
        Pointer target = _get< Pointer >( instruction().operand( 0 ) );
        jumpTo( *reinterpret_cast< PC * >( dereference( target ) ) );
    }

    /*
     * Two-phase PHI handler. We do this because all of the PHI nodes must be
     * executed atomically, reading their inputs before any of the results are
     * updated.  Not doing this can cause problems if the PHI nodes depend on
     * other PHI nodes for their inputs.  If the input PHI node is updated
     * before it is read, incorrect results can happen.
     */

    void switchBB( PC target )
    {
        PC origin = ccontext.pc();
        target.masked = origin.masked;
        ccontext.pc() = target;
        ccontext.jumped = true;

        _instruction = &info.instruction( ccontext.pc() );
        ASSERT( instruction().op );

        if ( !isa< ::llvm::PHINode >( instruction().op ) )
            return;  // Nothing fancy to do

        machine::Frame &original = ccontext.frame();
        int framesize = original.framesize( info );
        std::vector<char> tmp;
        tmp.resize( sizeof( machine::Frame ) + framesize );
        machine::Frame &copy = *reinterpret_cast< machine::Frame* >( &tmp[0] );
        copy = ccontext.frame();

        std::copy( original.memory(), original.memory() + framesize, copy.memory() );
        while ( ::llvm::PHINode *PN = dyn_cast< ::llvm::PHINode >( instruction().op ) ) {
            /* TODO use operands directly, avoiding valuemap lookup */
            auto v = info.valuemap[ PN->getIncomingValueForBlock(
                    cast< ::llvm::Instruction >( info.instruction( origin ).op )->getParent() ) ];
            FrameContext copyctx( info, copy );
            memcopy( v, instruction().result(), v.width, econtext, copyctx );
            ccontext.pc().instruction ++;
            _instruction = &info.instruction( ccontext.pc() );
        }
        std::copy( copy.memory(), copy.memory() + framesize, original.memory() );
    }

    /*
     * NB. Internally, we only deal with positive frameid's and they are always
     * relative to the *topmost* frame. The userspace doesn't know the stack
     * depth though, so we need to transform the frameid.
     */
    bool transform_frameid( int &frameid )
    {
        if ( frameid > 0 )
            // L - (L - id) - 1 = L - L + id - 1 = id - 1
            frameid = ccontext.stackDepth() - frameid;
        else
            frameid = std::min( frameid == INT_MIN ? INT_MAX : -frameid, ccontext.stackDepth() );

        if ( frameid > ccontext.stackDepth() ) {
            ccontext.problem( Problem::InvalidArgument );
            return false;
        }

        return true;
    }

    typedef std::vector< ProgramInfo::Value > Values;

    int find_invoke() {
        for ( int fid = 1; fid < ccontext.stackDepth(); ++fid ) {
            auto target = info.instruction( ccontext.frame( fid ).pc );
            if ( isa< ::llvm::InvokeInst >( target.op ) )
                return fid;
        }
        return ccontext.stackDepth(); /* past the end */
    }

    void unwind( int frameid, const Values &args )
    {
        if ( !transform_frameid( frameid ) )
            return;

        PC target_pc;

        /* we have a place to return to */
        if ( frameid < ccontext.stackDepth() ) {
            auto target = &info.instruction( ccontext.frame( frameid ).pc );

            if ( isa< ::llvm::InvokeInst >( target->op ) ) {
                target_pc = _get< PC >( ValueRef( target->operand( -1 ), frameid ) );
                target = &info.instruction( target_pc );
            }

            int copied = 0;
            for ( auto arg : args ) {
                memcopy( arg, ValueRef( target->result(), frameid, -1, copied ), arg.width );
                copied += arg.width;
            }
        }

        while ( frameid ) { /* jump out */
            ccontext.leave();
            -- frameid;
        }

        if ( target_pc.function )
            jumpTo( target_pc );

        maybe_die();
    }

    struct LPInfo {
        bool cleanup;
        Pointer person;
        std::vector< std::pair< int, Pointer > > items;
        LPInfo() : cleanup( false ) {}
    };

    LPInfo getLPInfo( PC pc, int frameid = 0 )
    {
        LPInfo r;
        auto lpi = info.instruction( pc );
        if ( auto LPI = dyn_cast< ::llvm::LandingPadInst >( lpi.op ) ) {
            r.cleanup = LPI->isCleanup();
            r.person = Pointer( _get< PC >( info.function( pc ).personality ) );
            ASSERT( !r.person.null() );
            for ( int i = 0; i < int( LPI->getNumClauses() ); ++i ) {
                if ( LPI->isFilter( i ) ) {
                    r.items.push_back( std::make_pair( -1, Pointer() ) );
                } else {
                    auto tag = _get< Pointer >( ValueRef( lpi.operand( i ), frameid ) );
                    auto type_id = info.function( pc ).typeID( tag );
                    r.items.push_back( std::make_pair( type_id, tag ) );
                }
            }
        }
        return r;
    }

    Pointer make_lpinfo( int frameid )
    {
        if ( !transform_frameid( frameid ) )
            return Pointer();
        if ( frameid == ccontext.stackDepth() )
            return Pointer();

        LPInfo lp;

        PC pc = ccontext.frame( frameid ).pc;
        auto insn = info.instruction( pc );
        if ( isa< ::llvm::InvokeInst >( insn.op ) ) {
            pc = _get< PC >( ValueRef( insn.operand( -1 ), frameid ) );
            lp = getLPInfo( pc, frameid );
        }

        /* Well, this is not very pretty. */
        int psize = info.TD.getPointerSize();
        Pointer r = econtext.malloc( 8 + psize + (4 + psize) * lp.items.size(), 0 );

        auto flagof = [&]( Pointer p ) -> MemoryFlag {
            if ( p.null() || p.code )
                return MemoryFlag::Data;
            auto pflag = econtext.memoryflag( p );
            if ( pflag.valid() )
                return pflag.get();
            return MemoryFlag::Data;
        };

        MemoryAccess acc( r, econtext );
        acc.setAndShift< int32_t >( lp.cleanup );
        acc.setAndShift< int32_t >( lp.items.size() );
        acc.setAndShift< Pointer >( lp.person, flagof( lp.person ), psize );

        for ( auto p : lp.items ) {
            acc.setAndShift< int32_t >( p.first );
            acc.setAndShift< Pointer >( p.second, flagof( p.second ), psize );
        }

        return r;
    }

    struct PointerBlock {
        int count;
        Pointer ptr[0];
    };

    void implement_intrinsic( int id ) {
        switch ( id ) {
            case Intrinsic::vastart:
            case Intrinsic::trap:
            case Intrinsic::vaend:
            case Intrinsic::vacopy:
                ASSERT_UNIMPLEMENTED(); /* TODO */
            case Intrinsic::eh_typeid_for: {
                auto tag = _get< Pointer >( instruction().operand( 0 ) );
                int type_id = info.function( ccontext.pc() ).typeID( tag );
                _get< int >( 0 ) = type_id;
                setFlags( instruction().result(), MemoryFlag::Data );
                return;
            }
            case Intrinsic::smul_with_overflow:
            case Intrinsic::sadd_with_overflow:
            case Intrinsic::ssub_with_overflow:
                is_signed = true;
            case Intrinsic::umul_with_overflow:
            case Intrinsic::uadd_with_overflow:
            case Intrinsic::usub_with_overflow: {
                auto res = instruction().result();
                res.width = instruction().operand( 0 ).width;
                res.type = ProgramInfo::Value::Integer;
                auto over = instruction().result();
                over.width = 1; // hmmm
                over.type = ProgramInfo::Value::Integer;
                over.offset += compositeOffset( instruction().op->getType(), 1 ).first;
                ASSERT_UNIMPLEMENTED();
                /* withValues( ArithmeticWithOverflow( id ), res, over,
                            instruction().operand( 0 ), instruction().operand(
                            1 ) ); */
                return;
            }
            case Intrinsic::stacksave:
            case Intrinsic::stackrestore: {
                std::vector< ProgramInfo::Instruction > allocas;
                std::vector< Pointer > ptrs;
                auto &f = info.functions[ ccontext.pc().function ];
                for ( auto &i : f.instructions )
                    if ( i.opcode == LLVMInst::Alloca ) {
                        allocas.push_back( i );
                        auto ptr = _get< Pointer >( i.result() );
                        if ( !ptr.null() )
                            ptrs.push_back( ptr );
                    }
                if ( id == Intrinsic::stacksave ) {
                    Pointer r = econtext.malloc( sizeof( PointerBlock ) + ptrs.size() * sizeof( Pointer ), 0 );
                    auto &c = *reinterpret_cast< PointerBlock * >( econtext.dereference( r ) );
                    c.count = ptrs.size();
                    int i = 0;
                    for ( auto ptr : ptrs ) {
                        econtext.memoryflag( r + sizeof( int ) + sizeof( Pointer ) * i ).set( MemoryFlag::HeapPointer );
                        c.ptr[ i++ ] = ptr;
                    }
                    _get< Pointer >( 0 ) = r;
                    setFlags( instruction().result(), MemoryFlag::HeapPointer );
                } else { // stackrestore
                    Pointer r = _get< Pointer >( instruction().operand( 0 ) );
                    auto &c = *reinterpret_cast< PointerBlock * >( econtext.dereference( r ) );
                    for ( auto ptr : ptrs ) {
                        bool retain = false;
                        for ( int i = 0; i < c.count; ++i )
                            if ( ptr == c.ptr[i] ) {
                                retain = true;
                                break;
                            }
                        if ( !retain )
                            econtext.free( ptr );
                    }
                }
                return;
            }
            default:
                /* We lowered everything else in buildInfo. */
                instruction().op->dump();
                ASSERT_UNREACHABLE_F( "unexpected intrinsic %d", id );
        }
    }

    void implement_builtin() {
        switch( instruction().builtin ) {
            case BuiltinChoice: {
                auto &c = ccontext.choice;
                c.options = _get< int >( instruction().operand( 0 ) );
                for ( int i = 1; i < int( instruction().values.size() ) - 2; ++i )
                    c.p.push_back( _get< int >( instruction().operand( i ) ) );
                if ( !c.p.empty() && int( c.p.size() ) != c.options ) {
                    ccontext.problem( Problem::InvalidArgument );
                    c.p.clear();
                }
                return;
            }
            case BuiltinAssert:
                if ( !_get< int >( instruction().operand( 0 ) ) )
                    ccontext.problem( Problem::Assert );
                return;
            case BuiltinProblem:
                ccontext.problem(
                    Problem::What( _get< int >( instruction().operand( 0 ) ) ),
                    _get< Pointer >( instruction().operand( 1 ) ) );
                return;
            case BuiltinAp:
                ccontext.flags().ap |= (1 << _get< int >( instruction().operand( 0 ) ));
                return;
            case BuiltinMask: ccontext.pc().masked = true; return;
            case BuiltinUnmask: ccontext.pc().masked = false; return;
            case BuiltinInterrupt: return; /* an observable noop, see interpreter.h */
            case BuiltinGetTID:
                _get< int >( 0 ) = ccontext.threadId();
                setFlags( instruction().result(), MemoryFlag::Data );
                return;
            case BuiltinNewThread: {
                PC entry = _get< PC >( instruction().operand( 0 ) );
                Pointer arg = _get< Pointer >( instruction().operand( 1 ) );
                auto mflag = econtext.memoryflag( instruction().operand( 1 ) );
                int tid = ccontext.new_thread(
                    entry, Maybe< Pointer >::Just( arg ),
                    mflag.valid() ? mflag.get() : MemoryFlag::Data );
                _get< int >( 0 ) = tid;
                setFlags( instruction().result(), MemoryFlag::Data );
                return;
            }
            case BuiltinMalloc: {
                int size = _get< int >( instruction().operand( 0 ) );
                if ( size >= ( 2 << Pointer::offsetSize ) ) {
                    ccontext.problem( Problem::InvalidArgument, Pointer() );
                    size = 0;
                }
                Pointer result = size ? econtext.malloc( size, pointerId( true )[0] ) : Pointer();
                _get< Pointer >( 0 ) = result;
                setFlags( instruction().result(), MemoryFlag::HeapPointer );
                return;
            }
            case BuiltinFree: {
                Pointer v = _get< Pointer >( instruction().operand( 0 ) );
                if ( !econtext.free( v ) )
                    ccontext.problem( Problem::InvalidArgument, v );
                return;
            }
            case BuiltinIsPrivate: {
                Pointer p = _get< Pointer >( 1 );
                _get< int >( 0 ) = econtext.isPrivate( ccontext.threadId(), p );
                setFlags( instruction().result(), MemoryFlag::Data );
                return;
            }
            case BuiltinHeapObjectSize: {
                Pointer v = _get< Pointer >( instruction().operand( 0 ) );
                if ( !econtext.dereference( v ) )
                    ccontext.problem( Problem::InvalidArgument, v );
                else {
                    _get< int >( 0 ) = econtext.pointerSize( v );
                    setFlags( instruction().result(), MemoryFlag::Data );
                }
                return;
            }
            case BuiltinMemcpy:
                if ( auto problem = memcopy( _get< Pointer >( 2 ), _get< Pointer >( 1 ), _get< size_t >( 3 ) ) )
                    ccontext.problem( problem );
                memcopy( instruction().operand( 1 ), instruction().result(), instruction().result().width );
                return;
            case BuiltinVaStart: {
                auto f = info.functions[ ccontext.pc().function ];
                memcopy( f.values[ f.argcount ], instruction().result(), instruction().result().width );
                return;
            }
            case BuiltinUnwind:
                return unwind( _get< int >( instruction().operand( 0 ) ),
                               Values( instruction().values.begin() + 2,
                                       instruction().values.end() - 1 ) );
            case BuiltinLandingPad:
                _get< Pointer >( 0 ) = make_lpinfo( _get< int >( instruction().operand( 0 ) ) );
                setFlags( instruction().result(), MemoryFlag::HeapPointer );
                return;
            default:
                ASSERT_UNREACHABLE_F( "unknown builtin %d", instruction().builtin );
        }
    }

    void implement_call() {
        CallSite CS( cast< ::llvm::Instruction >( instruction().op ) );
        ::llvm::Function *F = CS.getCalledFunction();

        if ( F && F->isDeclaration() ) {
            auto id = F->getIntrinsicID();
            if ( id != Intrinsic::not_intrinsic )
                return implement_intrinsic( id );

            if ( instruction().builtin )
                return implement_builtin();
        }

        bool invoke = isa< ::llvm::InvokeInst >( instruction().op );
        auto pc = _get< PC >( instruction().operand( invoke ? -3 : -1 ) );

        if ( !pc.function ) {
            ccontext.problem( Problem::InvalidDereference ); /* function 0 does not exist */
            return;
        }

        const auto &function = info.function( pc );

        /* report problems with the call before pushing the new stackframe */
        if ( !function.vararg && int( CS.arg_size() ) > function.argcount )
            ccontext.problem( Problem::InvalidArgument ); /* too many actual arguments */

        ccontext.enter( pc.function ); /* push a new frame */
        ccontext.jumped = true;

        /* Copy arguments to the new frame. */
        for ( int i = 0; i < int( CS.arg_size() ) && i < int( function.argcount ); ++i )
            memcopy( ValueRef( instruction().operand( i ), 1 ), function.values[ i ],
                     function.values[ i ].width );

        if ( function.vararg ) {
            int size = 0;
            for ( int i = function.argcount; i < int( CS.arg_size() ); ++i )
                size += instruction().operand( i ).width;
            Pointer vaptr = size ? econtext.malloc( size, 0 ) : Pointer();
            auto vaptr_loc = function.values[ function.argcount ];
            _get< Pointer >( vaptr_loc ) = vaptr;
            setFlags( vaptr_loc, MemoryFlag::HeapPointer );
            for ( int i = function.argcount; i < int( CS.arg_size() ); ++i ) {
                auto op = instruction().operand( i );
                memcopy( ValueRef( op, 1 ), vaptr, op.width );
                vaptr = vaptr + int( op.width );
            }
        }

        ASSERT( !isa< ::llvm::PHINode >( instruction().op ) );
    }

    void setFlags( ValueRef vr, MemoryFlag f ) {
        auto mflag = econtext.memoryflag( vr );
        if ( mflag.valid() ) {
            const int w = vr.v.width;
            if ( w ) {
                mflag.set( f );
                ++mflag;
                if ( f == MemoryFlag::HeapPointer )
                    f = MemoryFlag::Data;
                for ( int i = 1; i < w; ++i, ++mflag )
                    mflag.set( f );
            }
        }
    }

    auto operandFlag( int i ) {
        auto f = econtext.memoryflag( instruction().operand( i ) );
        return f.valid() ? f.get() : MemoryFlag::Data;
    }

    void binopFlags( bool propagate = true ) {
        if ( operandFlag( 0 ) == MemoryFlag::Uninitialised ||
             operandFlag( 1 ) == MemoryFlag::Uninitialised )
            setFlags( instruction().result(), MemoryFlag::Uninitialised );
        else if ( propagate && ( operandFlag( 0 ) == MemoryFlag::HeapPointer ||
                                 operandFlag( 1 ) == MemoryFlag::HeapPointer ) )
            setFlags( instruction().result(), MemoryFlag::HeapPointer );
        else
            setFlags( instruction().result(), MemoryFlag::Data );
    }

    void convertFlags() {
        if ( operandFlag( 0 ) == MemoryFlag::HeapPointer &&
             instruction().result().width < econtext.pointerTypeSize() )
            setFlags( instruction().result(), MemoryFlag::Data );
        else
            setFlags( instruction().result(), operandFlag( 0 ) );
    }

    void run() {

        /* operation templates */

        auto _cmp = [this] ( auto impl ) -> void {
            this->op< IntegerComparable >( 1, [&]( auto get ) {
                    impl( this->_get< bool >( 0 ), get( 1 ), get( 2 ) ); } );
        };

        auto _div = [this] ( auto impl ) -> void {
            this->binopFlags();
            this->op< std::is_arithmetic >( 0, [&]( auto get )
                {
                    if ( !get( 2 ) ) {
                        this->ccontext.problem( Problem::DivisionByZero );
                        get( 0 ) =
                            typename std::remove_reference< decltype( get( 0 ) ) >::type( 0 );
                    } else
                        impl( get( 0 ), get( 1 ), get( 2 ) );
                } );
        };

        auto _arith = [this] ( auto impl ) -> void {
            this->binopFlags();
            this->op< std::is_arithmetic >( 0, [&]( auto get )
                   { impl( get( 0 ), get( 1 ), get( 2 ) ); } );
        };

        auto _bitwise = [this] ( auto impl ) -> void {
            this->binopFlags();
            this->op< std::is_integral >( 0, [&]( auto get )
                   { impl( get( 0 ), get( 1 ), get( 2 ) ); } );
        };

        auto _atomicrmw = [this] ( auto impl ) -> void {
            this->op< std::is_integral >( 0, [&]( auto get ) {
                    auto &edit = this->_deref< decltype( get( -1 ) ) >( 1 );
                    get( 0 ) = edit;
                    impl( edit, get( 2 ) );
                } );
        };

        auto _cmp_signed = [this] ( auto impl ) -> void {
            this->op< std::is_integral >( 1, [&]( auto get ) {
                    impl( this->_get< bool >( 0 ), make_signed( get( 1 ) ),
                          make_signed( get( 2 ) ) ); } );
        };

        auto _fcmp = [&]( auto impl ) -> void {
            this->op< std::is_floating_point >( 1, [&]( auto get ) {
                    impl( this->_get< bool >( 0 ), get( 1 ), get( 2 ) ); } );
        };

        /* instruction dispatch */

        switch ( instruction().opcode )
        {

            case LLVMInst::GetElementPtr:
                setFlags( instruction().result(), operandFlag( 0 ) );
                _get< Pointer >( 0 ) = _get< Pointer >( 1 ) + compositeOffsetFromInsn(
                    instruction().op->getOperand(0)->getType(), 1, instruction().values.size() - 1 );
                return;

            case LLVMInst::Select: {
                int selected = _get< bool >( 1 ) ? 1 : 2;

                if ( operandFlag( 0 ) == MemoryFlag::Uninitialised ) {
                    ccontext.problem( Problem::Uninitialised );
                    setFlags( instruction().result(), MemoryFlag::Uninitialised );
                } else
                    setFlags( instruction().result(), operandFlag( selected ) );

                memcopy( instruction().operand( selected ), instruction().result(), instruction().result().width );
                return;
            }

            case LLVMInst::ICmp: {

                binopFlags( false );

                auto p = cast< ICmpInst >( instruction().op )->getPredicate();
                switch ( p ) {
                    case ICmpInst::ICMP_EQ:
                        return _cmp( []( auto &r, auto &a, auto &b ) { r = a == b; } );
                    case ICmpInst::ICMP_NE:
                        return _cmp( []( auto &r, auto &a, auto &b ) { r = a != b; } );
                    case ICmpInst::ICMP_ULT:
                        return _cmp( []( auto &r, auto &a, auto &b ) { r = a < b; } );
                    case ICmpInst::ICMP_UGE:
                        return _cmp( []( auto &r, auto &a, auto &b ) { r = a >= b; } );
                    case ICmpInst::ICMP_UGT:
                        return _cmp( []( auto &r, auto &a, auto &b ) { r = a > b; } );
                    case ICmpInst::ICMP_ULE:
                        return _cmp( []( auto &r, auto &a, auto &b ) { r = a <= b; } );
                    case ICmpInst::ICMP_SLT:
                        return _cmp_signed( []( auto &r, auto &a, auto &b ) { r = a < b; } );
                    case ICmpInst::ICMP_SGT:
                        return _cmp_signed( []( auto &r, auto &a, auto &b ) { r = a > b; } );
                    case ICmpInst::ICMP_SLE:
                        return _cmp_signed( []( auto &r, auto &a, auto &b ) { r = a <= b; } );
                    case ICmpInst::ICMP_SGE:
                        return _cmp_signed( []( auto &r, auto &a, auto &b ) { r = a >= b; } );
                    default: ASSERT_UNREACHABLE_F( "unexpected icmp op %d", p );
                }
            }


            case LLVMInst::FCmp: {

                binopFlags( false );

                auto p = cast< FCmpInst >( instruction().op )->getPredicate();
                bool nan;

                _fcmp( [&]( auto &, auto &a, auto &b )
                       { nan = isnan( a ) || isnan( b ); } );

                switch ( p ) {
                    case FCmpInst::FCMP_FALSE:
                        _get< bool >( 0 ) = false; return;
                    case FCmpInst::FCMP_TRUE:
                        _get< bool >( 0 ) = true; return;
                    case FCmpInst::FCMP_ORD:
                        _get< bool >( 0 ) = !nan; return;
                    case FCmpInst::FCMP_UNO:
                        _get< bool >( 0 ) = nan; return;
                    default: ;
                }

                if ( nan )
                    switch ( p ) {
                        case FCmpInst::FCMP_UEQ:
                        case FCmpInst::FCMP_UNE:
                        case FCmpInst::FCMP_UGE:
                        case FCmpInst::FCMP_ULE:
                        case FCmpInst::FCMP_ULT:
                        case FCmpInst::FCMP_UGT:
                            _get< bool >( 0 ) = true;
                            return;
                        case FCmpInst::FCMP_OEQ:
                        case FCmpInst::FCMP_ONE:
                        case FCmpInst::FCMP_OGE:
                        case FCmpInst::FCMP_OLE:
                        case FCmpInst::FCMP_OLT:
                        case FCmpInst::FCMP_OGT:
                            _get< bool >( 0 ) = false;
                            return;
                        default: ;
                    }

                switch ( p ) {
                    case FCmpInst::FCMP_OEQ:
                    case FCmpInst::FCMP_UEQ:
                        return _fcmp( []( auto &r, auto &a, auto &b ) { r = a == b; } );
                    case FCmpInst::FCMP_ONE:
                    case FCmpInst::FCMP_UNE:
                        return _fcmp( []( auto &r, auto &a, auto &b ) { r = a != b; } );

                    case FCmpInst::FCMP_OLT:
                    case FCmpInst::FCMP_ULT:
                        return _fcmp( []( auto &r, auto &a, auto &b ) { r = a < b; } );

                    case FCmpInst::FCMP_OGT:
                    case FCmpInst::FCMP_UGT:
                        return _fcmp( []( auto &r, auto &a, auto &b ) { r = a > b; } );

                    case FCmpInst::FCMP_OLE:
                    case FCmpInst::FCMP_ULE:
                        return _fcmp( []( auto &r, auto &a, auto &b ) { r = a <= b; } );

                    case FCmpInst::FCMP_OGE:
                    case FCmpInst::FCMP_UGE:
                        return _fcmp( []( auto &r, auto &a, auto &b ) { r = a >= b; } );
                    default:
                        ASSERT_UNREACHABLE_F( "unexpected fcmp op %d", p );
                }
            }

            case LLVMInst::ZExt:
            case LLVMInst::FPExt:
            case LLVMInst::UIToFP:
            case LLVMInst::FPToUI:
            case LLVMInst::PtrToInt:
            case LLVMInst::IntToPtr:
            case LLVMInst::FPTrunc:
            case LLVMInst::Trunc:

                convertFlags();
                return op< Convertible >( 0, 1, [this]( auto get0, auto get1 )
                           {
                               using T = typename std::remove_reference< decltype( get0( 0 ) ) >::type;
                               get0( 0 ) = T( get1( 1 ) );
                           } );
            case LLVMInst::SExt:
            case LLVMInst::SIToFP:
            case LLVMInst::FPToSI:
                convertFlags();
                return op< SignedConvertible >( 0, 1, [this]( auto get0, auto get1 )
                           {
                               using T = typename std::remove_reference<
                                   decltype( make_signed( get0( 0 ) ) ) >::type;
                               make_signed( get0( 0 ) ) = T( make_signed( get1( 1 ) ) );
                           } );

            case LLVMInst::Br:
                implement_br(); break;
            case LLVMInst::IndirectBr:
                implement_indirectBr(); break;
            case LLVMInst::Switch:
                if ( operandFlag( 0 ) == MemoryFlag::Uninitialised )
                    ccontext.problem( Problem::Uninitialised );
                return op< Any >( 1, [this]( auto get ) {
                        for ( int o = 2; o < int( this->instruction().values.size() ) - 1; o += 2 ) {
                            if ( operandFlag( o ) == MemoryFlag::Uninitialised )
                                ccontext.problem( Problem::Uninitialised );
                            if ( get( 1 ) == this->_get< typename std::remove_reference< decltype( get( 1 ) ) >::type >( o + 1 ) )
                                return this->jumpTo( this->instruction().operand( o + 1 ) );
                        }
                        return this->jumpTo( this->instruction().operand( 1 ) );
                    } );

            case LLVMInst::Call:
            case LLVMInst::Invoke:
                implement_call(); break;
            case LLVMInst::Ret:
                implement_ret(); break;

            case LLVMInst::BitCast:
                implement_bitcast(); break;

            case LLVMInst::Load:
                implement_load(); break;
            case LLVMInst::Store:
                implement_store(); break;
            case LLVMInst::Alloca:
                implement_alloca(); break;

            case LLVMInst::AtomicCmpXchg:
                setFlags( instruction().result(), MemoryFlag::Data ); /* TODO */
                return op< Any >( 2, [&]( auto get ) {
                        auto &r = get( 0 );
                        auto &pointer = this->_deref< decltype( get( -1 ) ) >( 1 );
                        auto &cmp = get( 2 );
                        auto &_new = get( 3 );

                        auto over = this->instruction().result();
                        over.width = 1; // hmmm
                        over.type = ProgramInfo::Value::Integer;
                        over.offset += this->compositeOffset( this->instruction().op->getType(), 1 ).first;

                        r = pointer;
                        if ( pointer == cmp ) {
                            pointer = _new;
                            this->_get< bool >( over ) = true;
                        } else
                            this->_get< bool >( over ) = false;
                    } );

            case LLVMInst::AtomicRMW:
                setFlags( instruction().result(), MemoryFlag::Data ); /* TODO */
                switch ( cast< AtomicRMWInst >( instruction().op )->getOperation() )
                {
                    case AtomicRMWInst::Xchg:
                        return _atomicrmw( []( auto &v, auto &x ) { v = x; } );
                    case AtomicRMWInst::Add:
                        return _atomicrmw( []( auto &v, auto &x ) { v = v + x; } );
                    case AtomicRMWInst::Sub:
                        return _atomicrmw( []( auto &v, auto &x ) { v = v - x; } );
                    case AtomicRMWInst::And:
                        return _atomicrmw( []( auto &v, auto &x ) { v = v & x; } );
                    case AtomicRMWInst::Nand:
                        return _atomicrmw( []( auto &v, auto &x ) { v = ~v & x; } );
                    case AtomicRMWInst::Or:
                        return _atomicrmw( []( auto &v, auto &x ) { v = v | x; } );
                    case AtomicRMWInst::Xor:
                        return _atomicrmw( []( auto &v, auto &x ) { v = v ^ x; } );
                    case AtomicRMWInst::UMax:
                        return _atomicrmw( []( auto &v, auto &x ) { v = std::max( v, x ); } );
                    case AtomicRMWInst::Max:
                        return _atomicrmw( []( auto &v, auto &x ) { make_signed( v ) =
                                    std::max( make_signed( v ), make_signed( x ) ); } );
                    case AtomicRMWInst::UMin:
                        return _atomicrmw( []( auto &v, auto &x ) { v = std::min( v, x ); } );
                    case AtomicRMWInst::Min:
                        return _atomicrmw( []( auto &v, auto &x ) { make_signed( v ) =
                                    std::min( make_signed( v ), make_signed( x ) ); } );
                    case AtomicRMWInst::BAD_BINOP:
                        ASSERT_UNREACHABLE_F( "bad binop in atomicrmw" );
                }

            case LLVMInst::ExtractValue:
                implement_extractvalue(); break;
            case LLVMInst::InsertValue:
                implement_insertvalue(); break;

            case LLVMInst::FAdd:
            case LLVMInst::Add:
                return _arith( []( auto &r, auto &a, auto &b ) { r = a + b; } );
            case LLVMInst::FSub:
            case LLVMInst::Sub:
                return _arith( []( auto &r, auto &a, auto &b ) { r = a - b; } );
            case LLVMInst::FMul:
            case LLVMInst::Mul:
                return _arith( []( auto &r, auto &a, auto &b ) { r = a * b; } );

            case LLVMInst::FDiv:
            case LLVMInst::UDiv:
                return _div( []( auto &r, auto &a, auto &b ) { r = a / b; } );

            case LLVMInst::SDiv:
			  return _div( []( auto &r, auto &a, auto &b ) {
                              return make_signed( r ) = make_signed( a ) / make_signed( b ); } );
            case LLVMInst::FRem:
            case LLVMInst::URem:
                return _div( []( auto &r, auto &a, auto &b ) { r = rem( a, b ); } );

            case LLVMInst::SRem:
                return _div( []( auto &r, auto &a, auto &b ) {
                        return make_signed( r ) = rem( make_signed( a ), make_signed( b ) ); } );

            case LLVMInst::And:
                return _bitwise( []( auto &r, auto &a, auto &b ) { r = a & b; } );
            case LLVMInst::Or:
                return _bitwise( []( auto &r, auto &a, auto &b ) { r = a | b; } );
            case LLVMInst::Xor:
                return _bitwise( []( auto &r, auto &a, auto &b ) { r = a ^ b; } );
            case LLVMInst::Shl:
                return _bitwise( []( auto &r, auto &a, auto &b ) { r = a << b; } );
            case LLVMInst::AShr:
                return _bitwise( []( auto &r, auto &a, auto &b ) { make_signed( r ) = make_signed( a ) >> b; } );
            case LLVMInst::LShr:
                return _bitwise( []( auto &r, auto &a, auto &b ) { r = a >> b; } );

            case LLVMInst::Unreachable:
                ccontext.problem( Problem::UnreachableExecuted );
                break;

            case LLVMInst::Resume:
                /* unwind down to the next invoke instruction in the stack */
                unwind( -find_invoke(), Values( { instruction().operand( 0 ) } ) );
                break;

            case LLVMInst::LandingPad:
                break; /* nothing to do, handled by the unwinder */

            case LLVMInst::Fence: /* noop until we have reordering simulation */
                break;

            default:
                instruction().op->dump();
                ASSERT_UNREACHABLE_F( "unknown opcode %d", instruction().opcode );
        }

        /* invoke and call are checked when ret executes */
        if ( instruction().opcode != LLVMInst::Call &&
             instruction().opcode != LLVMInst::Invoke )
            checkPointsTo();
    }
};


}
}

#endif
