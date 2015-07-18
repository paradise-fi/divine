// -*- C++ -*- (c) 2012-2014 Petr Ročkai <me@mornfall.net>

#include <brick-bitlevel.h>
#include <brick-types.h>
#include <brick-data.h>
#include <brick-assert.h>

#include <divine/toolkit/blob.h> // for align

#include <llvm/IR/Function.h>

#include <llvm/Config/config.h>
#if ( LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR < 2 )
  #include <llvm/Target/TargetData.h>
#else
  #include <llvm/IR/DataLayout.h>
  #define TargetData DataLayout
#endif

#include <llvm/CodeGen/IntrinsicLowering.h>

#include <map>
#include <unordered_map>

#ifndef DIVINE_LLVM_PROGRAM_H
#define DIVINE_LLVM_PROGRAM_H
#undef alloca

namespace llvm {
class IntrinsicLowering;
}

namespace divine {
namespace llvm {

using brick::bitlevel::bitcopy;
using brick::bitlevel::BitPointer;

struct Pointer;

struct PC : brick::types::Comparable
{
    uint32_t instruction:17;
    uint32_t function:13; /* must not be at end, since clang assumes that the
                             last bit of a function pointer is always 0 */
    bool masked:1;
    uint32_t code:1;

    PC( int f, int i )
        : instruction( i ), function( f ), masked( false ), code( 1 )
    {}
    PC() : PC( 0, 0 ) {}

    bool operator<= ( PC o ) const {
        /* masked is irrelevant for equality! */
        return std::make_tuple( int( function ), int( instruction ) )
            <= std::make_tuple( int( o.function ), int( o.instruction ) );
    }

    explicit PC( const uint32_t &x ) { *this = *reinterpret_cast< const PC * >( &x ); }
    explicit PC( const Pointer &p ) { *this = *reinterpret_cast< const PC * >( &p ); }

    explicit operator uint32_t() const {
        return *reinterpret_cast< const uint32_t * >( this );
    }
    explicit operator uint64_t() const { return uint32_t( *this ); }
};

/*
 * We use a *non-overlapping* segmented memory scheme, with *variable-sized
 * segments*. Each allocation creates a new segment. Pointers cannot cross
 * segment boundaries through pointer arithmetic or any other manipulation.
 */
struct Pointer : brick::types::Comparable
{
    static const uint32_t offsetSize = 14;
    static const uint32_t segmentSize = 16;

    uint32_t offset:offsetSize; // TODO use a bittuple for guaranteed layout; offset
                                // *must* be stored in the lowest 14 bits
    uint32_t segment:segmentSize;
    bool heap:1; /* make a (0, 0) pointer different from NULL */
    uint32_t code:1;
    Pointer operator+( int relative ) {
        return Pointer( heap, segment, offset + relative );
    }
    Pointer( bool heap, int segment, int offset )
        : offset( offset ), segment( segment ), heap( heap ), code( 0 )
    {}
    Pointer() : Pointer( false, 0, 0 ) {}
    bool null() { return !heap && !segment && !offset; }

    explicit operator uint32_t() const {
        return *reinterpret_cast< const uint32_t * >( this );
    }
    explicit operator uint64_t() const { return uint32_t( *this ); }

    explicit Pointer( uint32_t x ) {
        union U {
            uint32_t x;
            Pointer p;
            U( uint32_t x ) : x( x ) {}
        } u( x );
        *this = u.p;
    }
    explicit Pointer( const PC &x ) { *this = *reinterpret_cast< const Pointer * >( &x ); }

    bool operator<=( Pointer o ) const {
        return std::make_tuple( int( heap ), int( segment ), int( offset ) )
            <= std::make_tuple( int( o.heap ), int( o.segment ), int( o.offset ) );
    }
};

enum Builtin {
    NotBuiltin = 0,
    BuiltinIntrinsic = 1,
    BuiltinChoice,
    BuiltinMask,
    BuiltinUnmask,
    BuiltinInterrupt,
    BuiltinGetTID,
    BuiltinNewThread,
    BuiltinAssert,
    BuiltinMalloc,
    BuiltinFree,
    BuiltinHeapObjectSize,
    BuiltinAp,
    BuiltinMemcpy,
    BuiltinVaStart,
    BuiltinUnwind,
    BuiltinLandingPad,
    BuiltinProblem
};

struct Choice {
    int options;
    // might be empty or contain probability for each option
    std::vector< int > p;
};

struct ProgramInfo {
    ::llvm::IntrinsicLowering *IL;
    ::llvm::Module *module;
    ::llvm::TargetData TD;

    struct Value {
        enum { Void, Pointer, Integer, Float, Aggregate, CodePointer, Alloca } type:3;
        uint32_t width:29;
        bool constant:1;
        bool global:1;
        uint32_t offset:30;

        bool operator<( Value v ) const {
            return static_cast< uint32_t >( *this )
                 < static_cast< uint32_t >( v );
        }

        bool pointer() { return type == Pointer || type == Alloca; }
        bool alloca() { return type == Alloca; }
        bool integer() { return type == Integer; }
        bool isfloat() { return type == Float; }
        bool aggregate() { return type == Aggregate; }
        bool codePointer() { return type == CodePointer; }

        operator uint32_t() const {
            return *reinterpret_cast< const uint32_t * >( this );
        }

        Value()
            : type( Integer ), width( 0 ), constant( false ), global( false ), offset( 0 )
        {}
    };

    struct Instruction {
        unsigned opcode;
        brick::data::SmallVector< Value, 4 > values;
        Value &result() { return values[0]; }
        Value &operand( int i ) { return values[ (i >= 0) ? (i + 1) : (i + values.size()) ]; }

        int builtin; /* non-zero if this is a call to a builtin */
        ::llvm::User *op; /* the actual operation; Instruction or ConstantExpr */
        Instruction() : builtin( NotBuiltin ), op( nullptr ) {}
        /* next instruction is in the same BB unless op == NULL */
    };

    struct Function {
        int datasize;
        int argcount:31;
        bool vararg:1;
        std::vector< Value > values;
        std::vector< Instruction > instructions;
        std::vector< Pointer > typeIDs; /* for landing pads */
        int typeID( Pointer p )
        {
            auto found = std::find( typeIDs.begin(), typeIDs.end(), p );
            return found == typeIDs.end() ? 0 : 1 + (found - typeIDs.begin());
        }
        Instruction &instruction( PC pc ) {
            ASSERT_LEQ( int( pc.instruction ), int( instructions.size() ) - 1 );
            return instructions[ pc.instruction ];
        }
        Function() : datasize( 0 ) {}
    };

    std::vector< Function > functions;
    std::vector< Value > globals;
    std::vector< std::pair< int, Value > > globalvars;
    std::vector< std::pair< ::llvm::Type *, std::string > > globalinfo, constinfo;
    std::vector< char > constdata;
    std::vector< char > globaldata; /* initial values! */
    int globalsize, constdatasize;
    int framealign;
    bool codepointers;

    std::map< const ::llvm::Value *, Value > valuemap;
    std::map< const ::llvm::Instruction *, PC > pcmap;
    std::map< const ::llvm::Value *, std::string > anonmap;

    std::map< const ::llvm::BasicBlock *, PC > blockmap;
    std::map< const ::llvm::Function *, int > functionmap;

    int pointerTypeSize;

    template< typename Container >
    static void makeFit( Container &c, int index ) {
        c.resize( std::max( index + 1, int( c.size() ) ) );
    }

    Instruction &instruction( PC pc ) {
        return function( pc ).instruction( pc );
    }

    Function &function( PC pc ) {
        ASSERT_LEQ( int( pc.function ), int( functions.size() ) - 1 );
        return functions[ pc.function ];
    }

    template< typename T >
    T &constant( Value v ) {
        return *reinterpret_cast< T * >( &constdata[ v.offset ] );
    }

    char *allocateConstant( Value &result ) {
        result.constant = true;
        result.offset = constdatasize;
        constdatasize += result.width;
        constdata.resize( constdata.size() + result.width, 0 );
        return &constdata[ result.offset ];
    }

    using Coverage = std::vector< std::vector< ::llvm::Value * > >;
    std::vector< Coverage > coverage;

    bool lifetimeOverlap( ::llvm::Value *a, ::llvm::Value *b );
    bool overlayValue( int fun, Value &result, ::llvm::Value *val );

    void allocateValue( int fun, Value &result, ::llvm::Value *val = nullptr ) {
        result.constant = false;
        if ( fun ) {
            result.global = false;
            if ( !val || !overlayValue( fun, result, val ) ) {
                result.offset = functions[ fun ].datasize;
                functions[ fun ].datasize += result.width;
            }
        } else {
            result.global = true;
            result.offset = globalsize;
            globalsize += result.width;
            globaldata.resize( globalsize );
        }
    }

    template< typename T >
    void makeConstant( Value &result, T value ) {
        ASSERT_LEQ( sizeof( T ), result.width );
        allocateConstant( result );
        constant< T >( result ) = value;
    }

    std::deque< std::tuple< Value, ::llvm::Constant *, bool > > toInit;
    std::set< ::llvm::Value * > doneInit;

    void makeLLVMConstant( Value &result, ::llvm::Constant *c )
    {
        allocateConstant( result );
        valuemap.insert( std::make_pair( c, result ) );
        storeConstant( result, c );
    }

    bool isCodePointer( ::llvm::Value *val );
    bool isCodePointerConst( ::llvm::Value *val );
    PC getCodePointer( ::llvm::Value *val );

    Value storeConstantR( ::llvm::Constant *, bool & );
    void storeConstant( Value result, ::llvm::Constant *, bool global = false );

    bool globalPointerInBounds( Pointer p ) {
        ASSERT_LEQ( int( p.segment ), int( globals.size() ) - 1 );
        return p.offset < globals[ p.segment ].width;
    }

    int globalPointerOffset( Pointer p ) {
        ASSERT_LEQ( int( p.segment ), int( globals.size() ) - 1 );
        ASSERT( globalPointerInBounds( p ) );
        return globals[ p.segment ].offset + p.offset;
    }

    struct Position {
        PC pc;
        ::llvm::BasicBlock::iterator I;
        Position( PC pc, ::llvm::BasicBlock::iterator I ) : pc( pc ), I( I ) {}
    };

    template< typename Insn > void insertIndices( Position p );
    Position insert( Position );
    Position lower( Position ); // convert intrinsic into normal insns
    Builtin builtin( ::llvm::Function *f );
    void builtin( Position );
    void initValue( ::llvm::Value *val, Value &result );
    Value insert( int function, ::llvm::Value *val );
    void build();
    void pass();

    ProgramInfo( ::llvm::Module *m ) : module( m ), TD( m ), pointerTypeSize( TD.getPointerSize() )
    {
        constdatasize = 0;
        globalsize = 0;
        IL = new ::llvm::IntrinsicLowering( TD );
        build();
    }
};

struct ValueRef {
    ProgramInfo::Value v;
    int frame;
    int tid;
    int offset;
    ValueRef( ProgramInfo::Value v = ProgramInfo::Value(),
              int frame = 0, int tid = -1, int off = 0 )
        : v( v ), frame( frame ), tid( tid ), offset( off )
    {}
};

enum class MemoryFlag {
    Uninitialised, Data, HeapPointer
};

struct MemoryBits : BitPointer {
    static const int bitwidth = 2;

    MemoryBits( uint8_t *base = nullptr, int offset = 0 )
        : BitPointer( base, offset * bitwidth )
    {}

    union U {
        uint64_t x;
        volatile MemoryFlag t;
        U() : x() { }
    };

    void set( MemoryFlag s ) {
        static_assert( 32 % bitwidth == 0, "setUnsafe is only safe for divisors of 32" );
        setUnsafe( uint32_t( s ), bitwidth );
    }

    MemoryFlag get() {
        static_assert( 32 % bitwidth == 0, "getUnsafe is only safe for divisors of 32" );
        return MemoryFlag( getUnsafe( bitwidth ) );
    }

    MemoryBits &operator++() {
        shift( bitwidth );
        return *this;
    }

    MemoryBits operator++(int) {
        MemoryBits b = *this;
        shift( bitwidth );
        return b;
    }
};

inline bool isHeapPointer( MemoryBits f ) {
    return f.get() == MemoryFlag::HeapPointer;
};

struct GlobalContext {
    ProgramInfo &info;
    ::llvm::TargetData &TD;
    bool allow_global;

    Pointer malloc( int, int ) { ASSERT_UNREACHABLE( "" ); }
    bool free( Pointer ) { ASSERT_UNREACHABLE( "" ); }
    int pointerSize( Pointer ) { ASSERT_UNREACHABLE( "" ); }

    std::vector< int > pointerId( ::llvm::Instruction * ) {
        ASSERT_UNREACHABLE( "no pointerId in global context" );
    }
    int pointerId( Pointer ) {
        ASSERT_UNREACHABLE( "no pointerId in global context" );
    }

    MemoryBits memoryflag( Pointer ) { return MemoryBits(); }
    MemoryBits memoryflag( ValueRef ) { return memoryflag( Pointer() ); }

    void dump() {}

    /* TODO */
    bool inBounds( ValueRef, int ) { return true; }
    bool inBounds( Pointer, int ) { return true; }
    bool validate( Pointer, bool ) { return true; }

    char *dereference( Pointer p ) {
        if ( !p.heap && allow_global )
            return &info.globaldata[ info.globalPointerOffset( p ) ];
        ASSERT_UNREACHABLE( "dereferencing invalid pointer in GlobalContext" );
    }

    char *dereference( ValueRef v ) {
        if( v.v.constant )
            return &info.constdata[ v.v.offset + v.offset ];
        else if ( v.v.global && allow_global )
            return &info.globaldata[ v.v.offset + v.offset ];
        else
            ASSERT_UNREACHABLE( "dereferencing invalid value in GlobalContext" );
    }

    int pointerTypeSize() const { return info.pointerTypeSize; }

    GlobalContext( ProgramInfo &i, ::llvm::TargetData &TD, bool global )
        : info( i ), TD( TD ), allow_global( global )
    {}
};

std::ostream &operator<<( std::ostream &o, PC p );
std::ostream &operator<<( std::ostream &o, Pointer p );
std::ostream &operator<<( std::ostream &o, ProgramInfo::Value p );
std::ostream &operator<<( std::ostream &o, ValueRef p );

}
}

namespace std {
template<> struct hash< divine::llvm::PC > {
    size_t operator()( divine::llvm::PC pc ) const { return uint32_t( pc ); }
};
template<> struct hash< divine::llvm::Pointer > {
    size_t operator()( divine::llvm::Pointer p ) const { return uint32_t( p ); }
};
}

#endif
