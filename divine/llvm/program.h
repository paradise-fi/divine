// -*- C++ -*- (c) 2012 Petr Rockai

#include <wibble/mixin.h>
#include <wibble/test.h>
#include <divine/toolkit/blob.h> // for align

#include <llvm/Function.h>

#include <llvm/Config/config.h>
#if ( LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR < 2 )
  #include <llvm/Target/TargetData.h>
#else
  #include <llvm/DataLayout.h>
  #define TargetData DataLayout
#endif

#include <llvm/CodeGen/IntrinsicLowering.h>
#include <map>

#ifndef DIVINE_LLVM_PROGRAM_H
#define DIVINE_LLVM_PROGRAM_H

namespace llvm {
class IntrinsicLowering;
}

namespace divine {
namespace llvm {

struct MachineState;

struct PC : wibble::mixin::Comparable< PC >
{
    uint32_t code:1;
    uint32_t function:12;
    uint32_t block:10;
    uint32_t instruction:8;
    bool masked:1;

    PC( int f, int b, int i )
        : code( 1 ), function( f ), block( b ), instruction( i ), masked( false )
    {}

    PC()
        : code( 1 ), function( 0 ), block( 0 ), instruction( 0 ), masked( false )
    {}

    bool operator<= ( PC o ) const {
        /* masked is irrelevant for equality! */
        return std::make_tuple( int( function ), int( block ), int( instruction ) )
            <= std::make_tuple( int( o.function ), int( o.block ), int( o.instruction ) );
    }
};

/*
 * We use a *non-overlapping* segmented memory scheme, with *variable-sized
 * segments*. Each allocation creates a new segment. Pointers cannot cross
 * segment boundaries through pointer arithmetic or any other manipulation.
 */
struct Pointer : wibble::mixin::Comparable< Pointer > {
    uint32_t code:1;
    uint32_t offset:14;
    uint32_t segment:16;
    bool heap:1; /* make a (0, 0) pointer different from NULL */
    Pointer operator+( int relative ) {
        return Pointer( heap, segment, offset + relative );
    }
    Pointer( bool heap, int segment, int offset )
        : code( 0 ), offset( offset ), segment( segment ), heap( heap )
    {}
    Pointer() : code( 0 ), offset( 0 ), segment( 0 ), heap( false ) {}
    bool null() { return !heap && !segment; }

    operator uint32_t() const {
        return *reinterpret_cast< const uint32_t * >( this );
    }

    Pointer( uint32_t x ) {
        *reinterpret_cast< uint32_t * >( this ) = x;
    }

    /* For conversion of function pointers to plain pointers. */
    Pointer( PC x ) {
        std::copy( reinterpret_cast< char * >( &x ),
                reinterpret_cast< char * >( &x ) + sizeof( x ),
                reinterpret_cast< char* >( this ) );
    }

    operator PC() const {
        assert( code );
        return *reinterpret_cast< const PC * >( this );
    }

    Pointer &operator=( PC x ) {
        assert( x.code );
        std::copy( reinterpret_cast< char * >( &x ),
                reinterpret_cast< char * >( &x ) + sizeof( x ),
                reinterpret_cast< char* >( this ) );
        return *this;
    }

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
    BuiltinAp,
    BuiltinMemcpy
};

struct ProgramInfo {
    ::llvm::IntrinsicLowering *IL;
    ::llvm::Module *module;
    ::llvm::TargetData TD;

    struct Value {
        enum { Void, Pointer, Integer, Float, Aggregate, CodePointer } type:3;
        uint32_t width:29;
        bool constant:1;
        bool global:1;
        uint32_t offset:30;

        bool operator<( Value v ) const {
            return static_cast< uint32_t >( *this )
                 < static_cast< uint32_t >( v );
        }

        bool pointer() { return type == Pointer; }
        bool integer() { return type == Integer; }
        bool isfloat() { return type == Float; }
        bool aggregate() { return type == Aggregate; }
        bool codePointer() { return type == CodePointer; }

        operator uint32_t() const {
            return *reinterpret_cast< const uint32_t * >( this );
        }

        Value()
            : global( false ), constant( false ), type( Integer ),
              offset( 0 ), width( 0 )
        {}
    };

    struct Instruction {
        unsigned opcode;
        std::vector< Value > values;
        Value &result() { return values[0]; }
        Value &operand( int i ) { return values[ (i >= 0) ? (i + 1) : (i + values.size()) ]; }
        void dump( ProgramInfo &info, MachineState &state );

        int builtin; /* non-zero if this is a call to a builtin */
        ::llvm::User *op; /* the actual operation; Instruction or ConstantExpr */
        Instruction() : builtin( NotBuiltin ), op( nullptr ) {}
        /* next instruction is in the same BB unless op == NULL */
    };

    struct BB {
        std::vector< Instruction > instructions;
        ::llvm::BasicBlock *bb;
        Instruction &instruction( PC pc ) {
            assert_leq( int( pc.instruction ), int( instructions.size() ) - 1 );
            return instructions[ pc.instruction ];
        }
        BB() : bb( nullptr ) {}
    };

    struct Function {
        int datasize;
        std::vector< Value > values;
        std::vector< BB > blocks;
        BB &block( PC pc ) {
            assert_leq( int( pc.block ), int( blocks.size() ) - 1 );
            return blocks[ pc.block ];
        }
        Function() : datasize( 0 ) {}
    };

    std::vector< Function > functions;
    std::vector< Value > globals;
    std::map< Pointer, std::pair< ::llvm::Type *, std::string > > globalinfo, constinfo;
    std::vector< char > constdata;
    int globalsize, constdatasize;
    int framealign;
    bool codepointers;

    std::map< const ::llvm::Value *, Value > valuemap;
    std::map< const ::llvm::Instruction *, PC > pcmap;

    std::map< const ::llvm::BasicBlock *, PC > blockmap;
    std::map< const ::llvm::Function *, int > functionmap;

    template< typename Container >
    static void makeFit( Container &c, int index ) {
        c.resize( std::max( index + 1, int( c.size() ) ) );
    }

    Instruction &instruction( PC pc ) {
        assert_leq( int( pc.block ), int( function( pc ).blocks.size() ) - 1 );
        return block( pc ).instruction( pc );
    }

    BB &block( PC pc ) {
        assert_leq( int( pc.block ), int( function( pc ).blocks.size() ) - 1 );
        return function( pc ).block( pc );
    }

    Function &function( PC pc ) {
        assert_leq( int( pc.function ), int( functions.size() ) - 1 );
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

    void allocateValue( int fun, Value &result ) {
        result.constant = false;
        if ( fun ) {
            result.global = false;
            result.offset = functions[ fun ].datasize;
            functions[ fun ].datasize += result.width;
        } else {
            result.global = true;
            result.offset = globalsize;
            globalsize += result.width;
        }
    }

    template< typename T >
    void makeConstant( Value &result, T value ) {
        assert_leq( sizeof( T ), result.width );
        allocateConstant( result );
        constant< T >( result ) = value;
    }

    void makeLLVMConstant( Value &result, ::llvm::Constant *c )
    {
        allocateConstant( result );
        /* break loops in initializer dependencies */
        valuemap.insert( std::make_pair( c, result ) );
        storeConstant( result, c );
    }

    bool isCodePointer( ::llvm::Value *val );
    bool isCodePointerConst( ::llvm::Value *val );
    PC getCodePointer( ::llvm::Value *val );

    void storeConstant( Value &result, ::llvm::Constant *, char *global = nullptr );

    int globalPointerOffset( Pointer p ) {
        assert_leq( int( p.segment ), int( globals.size() ) - 1 );
        assert_leq( p.offset, globals[ p.segment ].width );
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

    ProgramInfo( ::llvm::Module *m ) : module( m ), TD( m )
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

struct GlobalContext {
    ProgramInfo &info;
    ::llvm::TargetData &TD;
    char *global;

    Pointer malloc( int ) { assert_die(); }
    void free( Pointer ) { assert_die(); }

    bool isPointer( ValueRef ) { return false; }
    bool isPointer( Pointer ) { return false; }
    void setPointer( ValueRef, bool ) {}
    void setPointer( Pointer, bool ) {}

    /* TODO */
    bool inBounds( ValueRef, int ) { return true; }
    bool inBounds( Pointer, int ) { return true; }

    char *dereference( Pointer p ) {
        if ( !p.heap )
            return global + info.globalPointerOffset( p );
        assert_die();
    }

    char *dereference( ValueRef v ) {
        if( v.v.constant )
            return &info.constdata[ v.v.offset + v.offset ];
        else if ( v.v.global )
            return global + v.v.offset + v.offset;
        else
            assert_die();
    }

    GlobalContext( ProgramInfo &i, ::llvm::TargetData &TD, char *global )
        : info( i ), TD( TD ), global( global )
    {}
};

std::ostream &operator<<( std::ostream &o, PC p );
std::ostream &operator<<( std::ostream &o, Pointer p );
std::ostream &operator<<( std::ostream &o, ProgramInfo::Value p );
std::ostream &operator<<( std::ostream &o, ValueRef p );

}
}

#endif
