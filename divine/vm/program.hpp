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
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/CallSite.h>
#include <llvm/CodeGen/IntrinsicLowering.h>
DIVINE_UNRELAX_WARNINGS

#include <divine/vm/pointer.hpp>
#include <divine/vm/heap.hpp>
#include <divine/vm/context.hpp>
#include <divine/cc/clang.hpp>

#include <runtime/divine.h>

#include <map>
#include <unordered_map>

#undef alloca

namespace divine {
namespace vm {

enum Hypercall /* see divine.h for prototypes & documentation */
{
    NotHypercall = 0,
    NotHypercallButIntrinsic = 1,

    HypercallControl,
    HypercallChoose,
    HypercallFault,
    HypercallInterruptCfl,
    HypercallInterruptMem,

    /* feedback */
    HypercallTrace,
    HypercallSyscall,

    /* memory management */
    HypercallObjMake,
    HypercallObjFree,
    HypercallObjShared,
    HypercallObjResize,
    HypercallObjSize
};

enum OpCodes { OpHypercall = llvm::Instruction::OtherOpsEnd + 1 };

struct Choice {
    int options;
    // might be empty or contain probability for each option
    std::vector< int > p;
};

static int intrinsic_id( llvm::Value *v )
{
    auto insn = llvm::dyn_cast< llvm::Instruction >( v );
    if ( !insn || insn->getOpcode() != llvm::Instruction::Call )
        return llvm::Intrinsic::not_intrinsic;
    llvm::CallSite CS( insn );
    auto f = CS.getCalledFunction();
    if ( !f )
        return llvm::Intrinsic::not_intrinsic;
    return f->getIntrinsicID();
}

/*
 * A representation of the LLVM program that is suitable for execution.
 */
struct Program
{
    llvm::Module *module;
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
        enum Type { Void, Pointer, Integer, Float, Aggregate, CodePointer, Alloca } type:3;
        /* NB. The numeric value of Location has to agree with the
           corresponding register's index in the _VM_ControlRegister enum */
        enum Location { Constant, Global, Local, Invalid } location:2;
        uint32_t _width:29;
        uint32_t offset:30;

        bool pointer() { return type == Pointer || type == Alloca; }
        bool alloca() { return type == Alloca; }
        bool integer() { return type == Integer; }
        bool isfloat() { return type == Float; }
        bool aggregate() { return type == Aggregate; }
        bool codePointer() { return type == CodePointer; }

        int size() const { return _width % 8 ? _width / 8 + 1 : _width / 8; }
        int width() const { return _width; }

        explicit Slot( Location l = Invalid, int w = 0 )
            : type( Integer ), location( l ), _width( w ), offset( 0 )
        {
        }
    };

    struct SlotRef
    {
        Slot slot;
        int seqno;
        explicit SlotRef( Slot s = Slot(), int n = -1 ) : slot( s ), seqno( n ) {}
    };

    struct Instruction
    {
        uint32_t opcode:16;
        uint32_t subcode:16;
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

        Instruction() : opcode( 0 ), subcode( 0 ) {}
    };

    struct Function
    {
        int framesize;
        int argcount:31;
        bool vararg:1;
        Slot personality;
        std::vector< Slot > values; /* TODO only store arguments; rename, use SmallVector */
        std::vector< Instruction > instructions;

        Instruction &instruction( CodePointer pc )
        {
            ASSERT_LT( pc.instruction(), instructions.size() );
            return instructions[ pc.instruction() ];
        }

        Function() : framesize( PointerBytes * 2 ) {}
    };

    std::vector< _VM_TypeTable > _types;
    std::vector< Function > functions;
    std::vector< Slot > _globals, _constants;
    int _globals_size, _constants_size;

    int framealign;
    bool codepointers;

    std::map< const llvm::Value *, SlotRef > valuemap;
    std::map< const llvm::Value *, SlotRef > globalmap;
    std::map< const llvm::Type *, int > typemap;

    std::map< const llvm::Instruction *, CodePointer > pcmap;
    std::map< CodePointer, llvm::Instruction * > insnmap;
    std::map< const llvm::Value *, std::string > anonmap;

    std::map< const llvm::BasicBlock *, CodePointer > blockmap;
    std::map< const llvm::Function *, int > functionmap;

    llvm::DITypeIdentifierMap ditypemap;

    using Context = ConstContext< Program, MutableHeap< 8 > >;
    Context _ccontext;

    auto &heap() { return _ccontext.heap(); }

    template< typename H >
    auto exportHeap( H &target )
    {
        auto cp = value::Pointer(
                heap::clone( _ccontext._heap, target, _ccontext.constants() ) );

        if ( !_globals_size )
            return std::make_pair( cp.cooked(), nullPointer() );

        auto gp = target.make( _globals_size );
        target.copy( _ccontext._heap, _ccontext.globals(),
                     gp.cooked(), _globals_size );
        target.shared( gp.cooked(), true );
        return std::make_pair( cp.cooked(), gp.cooked() );
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

    Function &function( llvm::Function *F )
    {
        return functions[ functionmap[ F ] ];
    }

    llvm::Function *llvmfunction( CodePointer pc ) /* eww. */
    {
        pc.instruction( 1 );
        return insnmap[ pc ]->getParent()->getParent();
    }

    int64_t allocsize( int id ) { return _types[ id ].type.size; }

    std::pair< int64_t, int > subtype( int id, int sub )
    {
        switch ( _types[ id ].type.type )
        {
            case _VM_Type::Array:
            {
                auto tid = _types[ id + 1 ].item.type_id;
                return std::make_pair( sub * allocsize( tid ), tid );
            }
            case _VM_Type::Struct:
            {
                ASSERT_LT( sub, _types[ id ].type.items );
                auto it = _types[ id + 1 + sub ].item;
                return std::make_pair( int64_t( it.offset ), it.type_id );
            }
            default:
                UNREACHABLE_F( "attempted to obtain an offset into a scalar, type = %d", id );
        }
    }

    SlotRef allocateSlot( Slot slot, int function = 0, llvm::Value *val = nullptr )
    {
        switch ( slot.location )
        {
            case Slot::Constant:
                slot.offset = _constants_size;
                _constants_size = mem::align( _constants_size + slot.size(), 4 );
                _constants.push_back( slot );
                return SlotRef( slot, _constants.size() - 1 );
            case Slot::Global:
                slot.offset = _globals_size;
                _globals_size = mem::align( _globals_size + slot.size(), 4 );
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

    GenericPointer s2ptr( SlotRef sr )
    {
        switch ( sr.slot.location )
        {
            case Slot::Constant: return ConstPointer( sr.seqno, 0 );
            case Slot::Global: return GlobalPointer( sr.seqno, 0 );
            default: UNREACHABLE( "invalid slot type in Program::s2ptr" );
        }
    }

    HeapPointer s2hptr( Slot s, int offset = 0 );

    using Coverage = std::vector< std::vector< llvm::Value * > >;
    std::vector< Coverage > coverage;

    bool lifetimeOverlap( llvm::Value *a, llvm::Value *b );
    void overlaySlot( int fun, Slot &result, llvm::Value *val );

    std::deque< std::function< void() > > _toinit;
    std::set< llvm::Value * > _doneinit;

    CodePointer functionByName( std::string s );
    GenericPointer globalByName( std::string s );

    bool isCodePointer( llvm::Value *val );
    bool isCodePointerConst( llvm::Value *val );
    CodePointer getCodePointer( llvm::Value *val );

    void initConstant( Slot slot, llvm::Value * );

    int getSubcode( llvm::Instruction *I )
    {
        if ( auto IC = llvm::dyn_cast< llvm::ICmpInst >( I ) )
            return IC->getPredicate();
        if ( auto FC = llvm::dyn_cast< llvm::FCmpInst >( I ) )
            return FC->getPredicate();
        if ( auto ARMW = llvm::dyn_cast< llvm::AtomicRMWInst >( I ) )
            return ARMW->getOperation();
        if ( auto INV = llvm::dyn_cast< llvm::InvokeInst >( I ) ) /* FIXME remove */
            return blockmap[ INV->getUnwindDest() ].instruction();

        UNREACHABLE( "bad instruction type in Program::getPredicate" );
    }

    int getSubcode( llvm::ConstantExpr *E ) { return E->getPredicate(); }

    template< typename IorCE >
    int initSubcode( IorCE *I )
    {
        if ( I->getOpcode() == llvm::Instruction::GetElementPtr )
            return insert( I->getOperand( 0 )->getType() );
        if ( I->getOpcode() == llvm::Instruction::ExtractValue )
            return insert( I->getOperand( 0 )->getType() );
        if ( I->getOpcode() == llvm::Instruction::InsertValue )
            return insert( I->getType() );
        if ( I->getOpcode() == llvm::Instruction::Alloca )
            return insert( I->getType()->getPointerElementType() );
        if ( I->getOpcode() == llvm::Instruction::Call )
            return intrinsic_id( I );
        if ( I->getOpcode() == llvm::Instruction::ICmp ||
             I->getOpcode() == llvm::Instruction::FCmp ||
             I->getOpcode() == llvm::Instruction::Invoke ||
             I->getOpcode() == llvm::Instruction::AtomicRMW )
            return getSubcode( I );
        return 0;
    }

    template< typename T >
    auto initConstant( Slot s, T t ) -> decltype( std::declval< typename T::Raw >(), void() )
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
    Hypercall hypercall( llvm::Function *f );
    void hypercall( Position );
    Slot initSlot( llvm::Value *val, Slot::Location loc );
    SlotRef insert( int function, llvm::Value *val );
    int insert( llvm::Type *t );

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
    Program( llvm::Module *m ) : module( m ), TD( m ), _ccontext( *this )
    {
        _constants_size = 0;
        _globals_size = 0;
    }
};

static inline std::ostream &operator<<( std::ostream &o, Program::Slot p )
{
    static std::vector< std::string > t = { "void", "ptr", "int", "float", "agg", "code", "alloca" };
    static std::vector< std::string > l = { "const", "global", "local", "invalid" };
    return o << "[" << l[ p.location ] << " " << t[ p.type ] << " @" << p.offset << " ↔"
             << p.width() << "]";
}

static inline std::ostream &operator<<( std::ostream &o, const Program::Instruction &i )
{
    for ( auto v : i.values )
        o << v << " ";
    return o;
}

}

namespace t_vm {

namespace {

auto testContext() {
    static std::shared_ptr< llvm::LLVMContext > ctx( new llvm::LLVMContext );
    return ctx;
}

auto mod2prog( std::unique_ptr< llvm::Module > m )
{
    auto p = std::make_shared< vm::Program >( m.release() );
    p->setupRR();
    p->computeRR();
    p->computeStatic();
    return p;
}

template< typename Build >
auto ir2prog( Build build, std::string funcname, llvm::FunctionType *ft = nullptr )
{
    if ( !ft )
        ft = llvm::FunctionType::get( llvm::Type::getInt32Ty( *testContext() ), false );
    auto m = std::make_unique< llvm::Module >( "test.ll", *testContext() );
    auto f = llvm::cast< llvm::Function >( m->getOrInsertFunction( funcname, ft ) );
    auto bb = llvm::BasicBlock::Create( *testContext(), "_entry", f );
    llvm::IRBuilder<> irb( bb );
    build( irb, f );
    return mod2prog( std::move( m ) );
}

auto c2prog( std::string s )
{
    divine::cc::Compiler c( testContext() );
    c.mapVirtualFile( "main.c", s );
    return mod2prog( c.compileModule( "main.c" ) );
}

}

struct Program
{
    llvm::LLVMContext &ctx;
    Program() : ctx( llvm::getGlobalContext() ) {}

    TEST( empty )
    {
        auto m = std::make_unique< llvm::Module >( "test", ctx );
        vm::Program p( m.get() );
    }

    TEST( simple )
    {
        auto m = c2prog( "int main() { return 0; }" );
    }
};

}

}
