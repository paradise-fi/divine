// -*- C++ -*- (c) 2012 Petr Rockai

#include <wibble/mixin.h>
#include <wibble/test.h>

#include <llvm/Function.h>
#include <llvm/Target/TargetData.h>
#include <llvm/CodeGen/IntrinsicLowering.h>
#include <map>

#ifndef DIVINE_LLVM_PROGRAM_H
#define DIVINE_LLVM_PROGRAM_H

namespace llvm {
class IntrinsicLowering;
};

namespace divine {
namespace llvm {

struct Interpreter;

void static align( int &v, int a ) {
    if ( v % a )
        v += a - (v % a);
}

struct PC : wibble::mixin::Comparable< PC >
{
    bool masked:1;
    uint32_t function:11;
    uint32_t block:10;
    uint32_t instruction:10;
    explicit PC( int f = 0, int b = 0, int i = 0 )
        : function( f ), block( b ), instruction( i ), masked( false )
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
    bool heap:1; /* make a (0, 0) pointer different from NULL */
    uint32_t segment:16; /* at most 64k objects */
    uint32_t offset:15; /* each at most 32kB */
    Pointer operator+( int relative ) {
        return Pointer( heap, segment, offset + relative );
    }
    explicit Pointer( int global )
        : heap( false ), segment( 1 ), offset( global )
    {}
    Pointer( int segment, int offset )
        : heap( true ), segment( segment ), offset( offset )
    {}
    Pointer( bool heap, int segment, int offset )
        : heap( heap ), segment( segment ), offset( offset )
    {}
    Pointer() : heap( false ), segment( 0 ), offset( 0 ) {}
    bool null() { return !heap && !segment; }

    bool operator<=( Pointer o ) const {
        return std::make_tuple( int( heap ), int( segment ), int( offset ) )
            <= std::make_tuple( int( o.heap ), int( o.segment ), int( o.offset ) );
    }
};

static std::ostream &operator<<( std::ostream &o, Pointer p ) {
    return o << "<" << p.segment << ":" << p.offset << ">";
}

enum Builtin {
    NotBuiltin = 0,
    BuiltinChoice = 1,
    BuiltinMask = 2,
    BuiltinUnmask = 3,
    BuiltinGetTID = 4,
    BuiltinNewThread = 5
};

struct ProgramInfo {
    ::llvm::TargetData TD;
    ::llvm::IntrinsicLowering *IL;
    ::llvm::Module *module;

    struct Value {
        bool global:1;
        bool constant:1;
        enum { Pointer, Integer, Float, Aggregate, CodePointer, Void } type:3;
        uint32_t offset:18;
        uint32_t width:9;

        bool operator<( Value v ) const {
            return *reinterpret_cast< const uint32_t * >( this )
                 < *reinterpret_cast< const uint32_t * >( &v );
        }

        bool pointer() { return type == Pointer; }
        bool integer() { return type == Integer; }
        bool isfloat() { return type == Float; }
        bool aggregate() { return type == Aggregate; }
        bool codePointer() { return type == CodePointer; }

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

        int builtin; /* non-zero if this is a call to a builtin */
        ::llvm::User *op; /* the actual operation; Instruction or ConstantExpr */
        Instruction() : op( nullptr ), builtin( NotBuiltin ) {}
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
        int framesize;
        std::vector< Value > values;
        std::vector< BB > blocks;
        BB &block( PC pc ) {
            assert_leq( int( pc.block ), int( blocks.size() ) - 1 );
            return blocks[ pc.block ];
        }
        Function() : framesize( 0 ) {}
    };

    std::vector< Function > functions;
    std::vector< Value > globals;
    std::vector< char > constdata;
    int globalsize, constdatasize;

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
        constdata.resize( constdata.size() + result.width );
        return &constdata[ result.offset ];
    }

    void allocateValue( int fun, Value &result ) {
        result.constant = false;
        if ( fun ) {
            result.global = false;
            result.offset = functions[ fun ].framesize;
            functions[ fun ].framesize += result.width;
        } else {
            result.global = true;
            result.offset = globalsize;
            globalsize += result.width;
        }
    }

    template< typename T >
    void makeConstant( Value &result, T value ) {
        assert_eq( result.width, sizeof( T ) );
        allocateConstant( result );
        constant< T >( result ) = value;
    }

    void makeLLVMConstant( Value &result, ::llvm::Constant *c )
    {
        allocateConstant( result );
        storeConstant( result, c );
    }

    void storeConstant( Value &result, ::llvm::Constant *, char *global = nullptr );

    struct Position {
        PC pc;
        ::llvm::BasicBlock::iterator I;
        Position( PC pc, ::llvm::BasicBlock::iterator I ) : pc( pc ), I( I ) {}
    };

    Position insert( Position );
    Position lower( Position ); // convert intrinsic into normal insns
    void builtin( Position );
    void initValue( ::llvm::Value *val, Value &result );
    Value insert( int function, ::llvm::Value *val );
    void build();

    ProgramInfo( ::llvm::Module *m ) : module( m ), TD( m )
    {
        constdatasize = 0;
        globalsize = 0;
        IL = new ::llvm::IntrinsicLowering( TD );
        build();
    }
};

struct GlobalContext {
    ProgramInfo &info;
    ::llvm::TargetData &TD;
    char *global;

    Pointer malloc( int ) { assert_die(); }

    char *dereference( Pointer p ) {
        if ( !p.heap )
            return global + p.offset;
        assert_die();
    }

    char *dereference( ProgramInfo::Value v, int = 0 ) {
        if( v.constant )
            return &info.constdata[ v.offset ];
        else if ( v.global )
            return global + v.offset;
        else
            assert_die();
    }

    GlobalContext( ProgramInfo &i, ::llvm::TargetData &TD, char *global )
        : info( i ), TD( TD ), global( global )
    {}
};

}
}

#endif
