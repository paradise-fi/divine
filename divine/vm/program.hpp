// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2012-2016 Petr Ročkai <code@fixp.eu>
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

#include <brick-types>
#include <brick-data>
#include <brick-assert>
#include <brick-mem>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Function.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/CodeGen/IntrinsicLowering.h>
DIVINE_UNRELAX_WARNINGS

#include <divine/vm/pointer.hpp>
#include <divine/vm/heap.hpp>

#include <runtime/divine.h>

#include <map>
#include <unordered_map>

#undef alloca

namespace llvm {
class IntrinsicLowering;
}

namespace divine {
namespace vm {

enum Builtin /* see divine.h for prototypes & documentation */
{
    NotBuiltin = 0,
    BuiltinIntrinsic = 1,

    /* system entry points */
    BuiltinSetSched,
    BuiltinSetFault,
    BuiltinSetIfl,

    /* control flow */
    BuiltinChoose,
    BuiltinMask,
    BuiltinJump,
    BuiltinFault,

    /* feedback */
    BuiltinTrace,

    /* memory management */
    BuiltinMakeObject,
    BuiltinFreeObject,
    BuiltinMemcpy,

    /* introspection */
    BuiltinQueryVarargs,
    BuiltinQueryFrame,
    BuiltinQueryObjectSize,
    BuiltinQueryInstruction,
    BuiltinQueryVariable,
    BuiltinQueryFunction
};

struct Choice {
    int options;
    // might be empty or contain probability for each option
    std::vector< int > p;
};

struct ConstContext
{
    using Heap = MutableHeap;
    using HeapPointer = Heap::Pointer;
    using PointerV = value::Pointer< HeapPointer >;
    struct Control
    {
        PointerV _constants;
        PointerV _globals;
        PointerV frame() { return PointerV(); }
        void frame( PointerV ) {}
        PointerV globals() { return _globals; }
        PointerV constants() { return _constants; }
        void fault( _VM_Fault ) { NOT_IMPLEMENTED(); }
        bool mask( bool )  { NOT_IMPLEMENTED(); }
        template< typename I >
        int choose( int, I, I ) { NOT_IMPLEMENTED(); }
        void setSched( CodePointer ) { NOT_IMPLEMENTED(); }
        void setFault( CodePointer ) { NOT_IMPLEMENTED(); }
        void setIfl( PointerV ) { NOT_IMPLEMENTED(); }
        bool isEntryFrame( HeapPointer ) { NOT_IMPLEMENTED(); }
        void trace( std::string ) { NOT_IMPLEMENTED(); }
    };

    Heap _heap;
    Control _control;

    Heap &heap() { return _heap; }
    Control &control() { return _control; }
    void setup( int gds, int cds )
    {
        control()._constants = heap().make( cds );
        if ( gds )
            control()._globals = heap().make( gds );
    }
};

/*
 * A representation of the LLVM program that is suitable for execution.
 */
struct Program
{
    llvm::IntrinsicLowering *IL;
    std::shared_ptr< llvm::Module > module;
    llvm::DataLayout TD;

    /*
     * Values (data) used in a program are organised into blocks of memory:
     * frames (one for each function), globals (for each process) and constants
     * (immutable and shared across all processes). These blocks consist of
     * individual slots, one slot per value stored in the given block. Slots
     * are typed and they can overlap *if* the lifetimes of the values stored
     * in them do not. In this respect, slots behave like pointers into the
     * given memory block. All slots are assigned statically. All LLVM Values
     * are allocated into slots.
     */
    struct Slot
    {
        enum { Void, Pointer, Integer, Float, Aggregate, CodePointer, Alloca } type:3;
        enum Location { Global, Local, Constant, Invalid } location:2;
        uint32_t width:29;
        uint32_t offset:30;

        bool operator<( Slot v ) const
        {
            return static_cast< uint32_t >( *this )
                 < static_cast< uint32_t >( v );
        }

        bool pointer() { return type == Pointer || type == Alloca; }
        bool alloca() { return type == Alloca; }
        bool integer() { return type == Integer; }
        bool isfloat() { return type == Float; }
        bool aggregate() { return type == Aggregate; }
        bool codePointer() { return type == CodePointer; }

        explicit operator uint32_t() const
        {
            return *reinterpret_cast< const uint32_t * >( this );
        }

        Slot( int w = 0, Location l = Invalid )
            : type( Integer ), location( l ), width( w ), offset( 0 )
        {}
    };

    struct SlotRef
    {
        Slot slot;
        int seqno;
        SlotRef( Slot s = Slot(), int n = -1 ) : slot( s ), seqno( n ) {}
    };

    struct Instruction
    {
        uint32_t opcode:16;
        uint32_t builtin:16; /* non-zero if this is a call to a builtin */
        brick::data::SmallVector< Slot, 4 > values;
        Slot &result() { ASSERT( values.size() ); return values[0]; }
        Slot &operand( int i )
        {
            int idx = (i >= 0) ? (i + 1) : (i + values.size());
            ASSERT_LT( idx, values.size() );
            return values[ idx ];
        }
        Slot &value( int i )
        {
            int idx = (i >= 0) ? i : (i + values.size());
            ASSERT_LT( idx, values.size() );
            return values[ idx ];
        }

        /*
         * TODO, remove the 'op' pointer (to improve compactness)...
         * - alloca needs to get a virtual size parameter
         * - LLVM type needs to be passed for {extract,insert}value, getelementptr
         * - PHI incoming block addresses need to be represented
         * - icmp/fcmp predicates, atomicrmw operation
         * - callsite handling needs to be reworked
         */
        llvm::User *op; /* the actual operation; Instruction or ConstantExpr */
        Instruction() : builtin( NotBuiltin ), op( nullptr ) {}
    };

    struct Function
    {
        int datasize;
        int argcount:31;
        bool vararg:1;
        Slot personality;
        std::vector< Slot > values; /* TODO only store arguments; rename, use SmallVector */
        std::vector< Instruction > instructions;
        std::vector< ConstPointer > typeIDs; /* for landing pads */

        int typeID( ConstPointer p )
        {
            auto found = std::find( typeIDs.begin(), typeIDs.end(), p );
            return found == typeIDs.end() ? 0 : 1 + (found - typeIDs.begin());
        }

        Instruction &instruction( CodePointer pc )
        {
            ASSERT_LT( pc.instruction(), instructions.size() );
            return instructions[ pc.instruction() ];
        }

        Function() : datasize( 0 ) {}
    };

    std::vector< Function > functions;
    std::vector< Slot > _globals, _constants;
    int _globals_size, _constants_size;

    int framealign;
    bool codepointers;

    std::map< const llvm::Value *, SlotRef > valuemap;
    std::map< const llvm::Instruction *, CodePointer > pcmap;
    std::map< const llvm::Value *, std::string > anonmap;

    std::map< const llvm::BasicBlock *, CodePointer > blockmap;
    std::map< const llvm::Function *, int > functionmap;

    ConstContext _ccontext;

    template< typename H >
    auto exportHeap( H &target )
    {
        auto cp = target.make( _constants_size );
        target.copy( _ccontext._heap, _ccontext.control().constants(), cp, _constants_size );

        if ( !_globals_size )
            return std::make_pair( cp, decltype( cp )( nullPointer() ) );

        auto gp = target.make( _globals_size );
        target.copy( _ccontext._heap, _ccontext.control().globals(), gp, _globals_size );
        return std::make_pair( cp, gp );
    }

    template< typename Container >
    static void makeFit( Container &c, int index )
    {
        c.resize( std::max( index + 1, int( c.size() ) ) );
    }

    Instruction &instruction( CodePointer pc )
    {
        return function( pc ).instruction( pc );
    }

    Function &function( CodePointer pc )
    {
        ASSERT_LT( pc.function(), functions.size() );
        ASSERT( pc.function() > 0 );
        return functions[ pc.function() ];
    }

    SlotRef allocateSlot( Slot slot , int function = 0, llvm::Value *val = nullptr )
    {
        switch ( slot.location )
        {
            case Slot::Constant:
                slot.offset = _constants_size;
                _constants_size = mem::align( _constants_size + slot.width, 4 );
                _constants.push_back( slot );
                return SlotRef( slot, _constants.size() - 1 );
            case Slot::Global:
                slot.offset = _globals_size;
                _globals_size = mem::align( _globals_size + slot.width, 4 );
                _globals.push_back( slot );
                return SlotRef( slot, _globals.size() - 1 );
            case Slot::Local:
                ASSERT( function );
                overlaySlot( function, slot, val );
                functions[ function ].values.push_back( slot );
                return SlotRef( slot, 0 );
            default:
                UNREACHABLE( "invalid slot location" );
        }
    }

    GenericPointer<> s2ptr( SlotRef sr )
    {
        switch ( sr.slot.location )
        {
            case Slot::Constant: return ConstPointer( sr.seqno, 0 );
            case Slot::Global: return GlobalPointer( sr.seqno, 0 );
            default: UNREACHABLE( "invalid slot type in Program::s2ptr" );
        }
    }

    ConstContext::PointerV s2hptr( Slot s, int offset = 0 );

    using Coverage = std::vector< std::vector< llvm::Value * > >;
    std::vector< Coverage > coverage;

    bool lifetimeOverlap( llvm::Value *a, llvm::Value *b );
    void overlaySlot( int fun, Slot &result, llvm::Value *val );

    std::deque< std::function< void() > > _toinit;
    std::set< llvm::Value * > _doneinit;

    CodePointer functionByName( std::string s );

    bool isCodePointer( llvm::Value *val );
    bool isCodePointerConst( llvm::Value *val );
    CodePointer getCodePointer( llvm::Value *val );

    void initStatic( Slot slot, llvm::Value * );

    template< typename T >
    auto initStatic( Slot s, T t ) -> decltype( std::declval< typename T::Raw >(), void() )
    {
        // std::cerr << "initial constant value " << t << " at " << s2hptr( s ) << std::endl;
        _ccontext.heap().write( s2hptr( s ), t );
    }


    struct Position
    {
        CodePointer pc;
        llvm::BasicBlock::iterator I;
        Position( CodePointer pc, llvm::BasicBlock::iterator I ) : pc( pc ), I( I ) {}
    };

    template< typename Insn > void insertIndices( Position p );
    Position insert( Position );
    Position lower( Position ); // convert intrinsic into normal insns
    Builtin builtin( llvm::Function *f );
    void builtin( Position );
    Slot initSlot( llvm::Value *val, Slot::Location loc );
    SlotRef insert( int function, llvm::Value *val );
    SlotRef insert( int function, llvm::Value *val, Slot::Location sl );

    void pass(); /* internal */

    void setupRR();
    void computeRR(); /* RR = runtime representation */
    void computeStatic();

    /* the construction sequence is this:
       1) constructor
       2) setupRR
       3a) (optional) set up additional constant/global data
       3b) computeRR
       4) computeStatic */
    Program( std::shared_ptr< llvm::Module > m ) : module( m ), TD( m.get() )
    {
        _constants_size = 0;
        _globals_size = 0;
        IL = new llvm::IntrinsicLowering( TD );
    }
};

static inline std::ostream &operator<<( std::ostream &o, Program::Slot p )
{
    static std::vector< std::string > t = { "void", "ptr", "int", "float", "agg", "code", "alloca" };
    static std::vector< std::string > l = { "global", "local", "const", "invalid" };
    return o << "[" << l[ p.location ] << " " << t[ p.type ] << " @" << p.offset << " ↔"
             << p.width << "]";
}

static inline std::ostream &operator<<( std::ostream &o, const Program::Instruction &i )
{
    for ( auto v : i.values )
        o << v << " ";
    return o;
}

}
}

