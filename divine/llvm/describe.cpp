// -*- C++ -*- (c) 2011 Petr Rockai <me@mornfall.net>
// Describe the interpreter's state in a human-readable fashion.

#include <divine/llvm/interpreter.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"

#include <llvm/Config/config.h>
#if ( LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR < 2 )
  #include <llvm/Analysis/DebugInfo.h>
#else
  #include <llvm/DebugInfo.h>
#endif
#include <llvm/ADT/StringMap.h>
#include <llvm/Support/raw_ostream.h>

#pragma GCC diagnostic pop

using namespace llvm;
using namespace divine::llvm;

namespace divine {
namespace llvm {

struct Describe {

    typedef std::set< std::pair< Pointer, Type * > > DescribeSeen;

    DescribeSeen seen;
    std::vector< std::string > lines;
    bool detailed;
    int anonymous;
    Interpreter *interpreter;

    std::string pointer( Type *t, Pointer p );
    Describe( Interpreter *i, bool detailed ) :  detailed( detailed ), anonymous( 1 ), interpreter( i ) {}

    std::string all();
    std::string constdata();

    ::llvm::TargetData &TD() { return interpreter->TD; }
    MachineState &state() { return interpreter->state; }
    ProgramInfo &info() { return interpreter->info(); }

    template< typename Ptr > std::string aggregate( Type *t, Ptr where );

    template< typename Ptr > std::string value( Type *t, Ptr where );
    template< typename Ptr >
    std::string value( const ::llvm::Value *, ValueRef vref, Ptr p );
    template< typename Ptr >
    std::string value( std::pair< ::llvm::Type *, std::string >, ValueRef vref, Ptr p );

    bool boring( std::string n, bool fun = false ) {
        if ( n.empty() )
            return true;
        if ( !detailed && n.find( '.' ) != std::string::npos )
            return true;
        if ( n.length() >= 2 && n[0] == '_' && std::isupper( n[1] ) )
            return true;
        if ( fun && ( wibble::str::startsWith( n, "pthread_" ) ) )
             return true;
        return false;
    }

};

std::ostream &operator<<( std::ostream &o, PC p ) {
    return o << p.function << ":" << p.block << ":" << p.instruction;
}

std::ostream &operator<<( std::ostream &o, Pointer p ) {
    if ( p.code )
        return o << static_cast< PC >( p );
    return o << p.segment << ":" << p.offset;
}

std::ostream &operator<<( std::ostream &o, ProgramInfo::Value p ) {
    if ( p.constant )
        o << "c";
    else if ( p.global )
        o << "g";
    else
        o << "l";
    return o << p.type << "[" << p.width << "]@" << p.offset;
}

std::ostream &operator<<( std::ostream &o, ValueRef p ) {
    return o << p.v << "+" << p.offset << " [" << p.tid << ", " << p.frame << "]";
}

}
}

template< typename Ptr >
std::string Describe::aggregate( Type *t, Ptr where )
{
    char delim[2];
    std::vector< std::string > vec;

    if ( isa< StructType >( t ) ) {
        delim[0] = '{'; delim[1] = '}';
        int startoffset = where.offset;
        StructType *stru = cast< StructType >( t );
        const StructLayout *SLO = TD().getStructLayout( stru );
        int index = 0;
        for ( auto st = stru->element_begin(); st != stru->element_end(); ++ st )
        {
            vec.push_back( value( (*st), where ) );
            where.offset = startoffset + SLO->getElementOffset( index );
            ++ index;
        }
    }

    if ( isa< ArrayType >( t )) {
        delim[0] = '['; delim[1] = ']';
        const ArrayType *arr = cast< ArrayType >( t );
        for ( int i = 0; i < int( arr->getNumElements() ); ++ i )
        {
            vec.push_back( value( arr->getElementType(), where ) );
            where.offset += TD().getTypeAllocSize( arr->getElementType() );
        }
    }

    return wibble::str::fmt_container( vec, delim[0], delim[1] );
}

std::string Describe::pointer( Type *t, Pointer p )
{
    if ( p.null() )
        return "null";

    std::string ptr = wibble::str::fmt( p );
    std::string res;
    Type *pointeeTy = cast< PointerType >( t )->getElementType();
    if ( p.code ) {
        res = wibble::str::fmt( static_cast< PC >( p ) );
    } else if ( seen.count( std::make_pair( p, pointeeTy ) ) ) {
        res = ptr + " <...>";
    } else {
        seen.insert( std::make_pair( p, pointeeTy ) );
        if ( state().validate( p ) )
            res = "@(" + ptr + "| " + value( pointeeTy, p ) + ")";
        else
            res = "@(" + ptr + "| invalid)";
    }
    return res;
}

static std::string fmtInteger( char *where, int bits ) {
    if ( !where )
        return "<null>";

    switch ( bits ) {
        case 64: return wibble::str::fmt( *reinterpret_cast< int64_t* >( where ) );
        case 32: return wibble::str::fmt( *reinterpret_cast< int32_t* >( where ) );
        case 16: return wibble::str::fmt( *reinterpret_cast< int16_t* >( where ) );
        case 8: return wibble::str::fmt( int( *reinterpret_cast< int8_t * >( where ) ) );
        case 1: return wibble::str::fmt( *reinterpret_cast< bool * >( where ) );
        default: return "<" + wibble::str::fmt( bits ) + "-bit integer>";
    }
}

void updateWidth( ::llvm::TargetData TD, ValueRef &w, Type *t ) {
    w.v.width = TD.getTypeAllocSize( t );
}

void updateWidth( ::llvm::TargetData, Pointer, Type * ) {}

template< typename Ptr >
std::string Describe::value( Type *t, Ptr where )
{
    updateWidth( TD(), where, t );
    if ( t->isAggregateType() )
        return aggregate( t, where );
    if ( t->isPointerTy() ) {
        char *mem = interpreter->dereference( where );
        return pointer( t, mem ? *reinterpret_cast< Pointer * >( mem ) : Pointer() );
    }
    if ( t->isIntegerTy() )
        return fmtInteger( state().dereference( where ), TD().getTypeAllocSize( t ) * 8 );
    if ( t->getPrimitiveSizeInBits() )
        return "<weird scalar>";
    return "<weird type>";
}

template< typename Ptr >
std::string Describe::value( const ::llvm::Value *val, ValueRef vref, Ptr where )
{
    std::string name, value;

    Type *type = val->getType();
    auto vname = val->getValueName();

    if ( vname ) {
        name = vname->getKey();
    } else if ( type->isVoidTy() )
        ;
    else if ( detailed )
        name = "%" + wibble::str::fmt( anonymous++ );

    if ( boring( name ) || isa< BasicBlock >( val ) )
        return "";

    return this->value( std::make_pair( type, name ), vref, where );
}

template< typename Ptr >
std::string Describe::value( std::pair< ::llvm::Type *, std::string > val,
                             ValueRef vref, Ptr where )
{
    if ( where.null() && !vref.v.width )
        return "";

    ::llvm::Type *type = val.first;
    std::string name = val.second;
    std::string value;

    if ( !where.null() ) {
        value = this->value( type, where );
    } else { /* scalar */
        if ( type->isPointerTy() && state().isPointer( vref ) ) {
            char *mem = state().dereference( vref );
            value = pointer( type, *reinterpret_cast< Pointer * >( mem ) );
        } else if ( vref.v.type == ProgramInfo::Value::Aggregate )
            value = aggregate( type, vref );
        else
            value = fmtInteger( state().dereference( vref ), vref.v.width * 8 );
    }

    if ( !boring( name ) )
        lines.push_back( name + " = " + value );

    return name.empty() ? "" : name + " = " + value;
}

std::string fileline( const Instruction &insn )
{
    const LLVMContext &ctx = insn.getContext();
    const DebugLoc &loc = insn.getDebugLoc();
    DILocation des( loc.getAsMDNode( ctx ) );
    if ( des.getLineNumber() )
        return des.getFilename().str() +
               std::string( ":" ) +
               wibble::str::fmt( des.getLineNumber() );
    return "";
}

std::string locinfo( ProgramInfo &info, PC pc,
                     bool instruction = false,
                     Function **fun = nullptr )
{
    auto user = info.instruction( pc ).op;
    if ( !isa< ::llvm::Instruction >( user ) )
        return "<non-code location>";

    std::stringstream locs;
    Instruction &insn = *cast< Instruction >( user );
    Function *f = insn.getParent()->getParent();
    std::string fl = fileline( insn );
    locs << "<" << f->getName().str() << ">";

    if ( fun )
        *fun = f;

    if ( !fl.empty() )
        locs << " [ " << fl << " ]";
    if ( fl.empty() || instruction ) {
        std::string descr;
        raw_string_ostream descr_stream( descr );
        descr_stream << insn;
        locs << " <<" << std::string( descr, 1, std::string::npos ) << " >>";
    }
    return locs.str();
}

std::string describeProblem( ProgramInfo &info, Problem bad )
{
    std::stringstream s;
    switch ( bad.what ) {
        case Problem::Assert:
            s << "ASSERTION FAILED"; break;
        case Problem::InvalidDereference:
            s << "BAD DEREFERENCE"; break;
        case Problem::InvalidArgument:
            s << "BAD ARGUMENT"; break;
        case Problem::OutOfBounds:
            s << "BOUND CHECK FAILED"; break;
        case Problem::DivisionByZero:
            s << "DIVISION BY ZERO"; break;
    }
    s << " (thread " << int( bad.tid ) << "): ";
    s << locinfo( info, bad.where, bad.what != Problem::Assert );
    return s.str();
}

std::string Describe::all()
{
    std::stringstream s;

    for ( int i = 0; i < int( info().globalinfo.size() ); ++ i )
        if ( info().globalinfo[ i ].first )
            this->value( info().globalinfo[ i ], ValueRef(), Pointer( false, i, 0 ) );

    if ( lines.size() )
        s << "global: " << wibble::str::fmt( lines ) << std::endl;

    for ( int c = 0; c < state()._thread_count; ++c ) {
        std::string location;
        bool bored = false;
        lines.clear();

        if ( state().stack( c ).get().length() &&
             info().instruction( state().frame( c ).pc ).op )
        {
            Function *f = nullptr;
            location = locinfo( info(), state().frame( c ).pc, false, &f );

            if ( f ) {
                bored = boring( f->getName(), true );
                for ( auto arg = f->arg_begin(); arg != f->arg_end(); ++ arg )
                    value( &*arg, ValueRef( info().valuemap[ &*arg ], 0, c ), Pointer() );

                for ( auto block = f->begin(); block != f->end(); ++block ) {
                    value( &*block, ValueRef(), Pointer() ); // just for counting
                    for ( auto v = block->begin(); v != block->end(); ++v )
                        value( &*v, ValueRef( info().valuemap[ &*v ], 0, c), Pointer() );
                }
            }
        } else
            location = "<null>";

        s << c << ": " << location << " " << ( bored ? "(internal)" : wibble::str::fmt( lines ) ) << std::endl;
    }

    MachineState::Flags &flags = state().flags();
    for ( int i = 0; i < flags.problemcount; ++i )
        s << describeProblem( info(), flags.problems(i) ) << std::endl;

    if ( !state()._thread_count )
        s << "EXIT" << std::endl;

    if ( flags.ap ) {
        int ap = flags.ap;
        std::vector< std::string > x;
        int k = 0;
        MDNode *apmeta = interpreter->findEnum( "AP" );
        while ( ap ) {
            if ( ap % 2 ) {
                MDString *name = cast< MDString >(
                    cast< MDNode >( apmeta->getOperand( k ) )->getOperand( 1 ) );
                x.push_back( name->getString() );
            }
            ap = ap >> 1;
            ++k;
        }
        s << "+ APs: " << wibble::str::fmt( x ) << std::endl;
    }

    return s.str();
}

std::string Describe::constdata() {
    for ( int i = 0; i < int( info().constinfo.size() ); ++ i )
        if ( info().constinfo[ i ].first )
            value( info().constinfo[ i ], ValueRef(), Pointer( false, i, 0 ) );

    return wibble::str::fmt( lines );
}

void MachineState::dump( std::ostream &r ) {

    /* TODO problem/flag stuff */

    r << "globals: [ ";
    for ( auto v = _info.globals.begin(); v != _info.globals.end(); v ++ )
        if ( !v->constant )
            r << fmtInteger( dereference( *v ), v->width * 8 ) << " ";
    r << "]" << std::endl;

    r << "constdata: [ ";
    for ( auto v = _info.globals.begin(); v != _info.globals.end(); v ++ )
        if ( v->constant )
            r << fmtInteger( dereference( *v ), v->width * 8 ) << " ";
    r << "]" << std::endl;


    r << "heap: segcount = " << heap().segcount << ", size = " << heap().size() << ", data = ";
    for ( int i = 0; i < heap().segcount; ++ i ) {
        Pointer p = Pointer( true, i, 0 );
        char *where = heap().dereference( Pointer( true, i, 0 ) );
        int size = heap().size( Pointer( true, i, 0 ) );
        for ( ; p.offset < size; p.offset += 4 ) {
            if ( validate( p ) && heap().isPointer( p ) ) {
                r << followPointer( p ) << " ";
            } else
                r << fmtInteger( where + p.offset, 32 ) << " ";
        }
        if ( i < heap().segcount - 1 )
            r << "| ";
    }
    r << std::endl;

    r << "nursery: segcount = " << nursery.offsets.size() - 1
      << ", size = " << nursery.offsets[ nursery.offsets.size() - 1 ]
      << ", data = ";
    for ( int i = 0; i < int( nursery.offsets.size() ) - 1; ++ i ) {
        Pointer p( true, i + nursery.segshift, 0 );
        char *where = nursery.dereference( p );
        int size = nursery.size( p );
        for ( ; p.offset < size; p.offset += 4 ) {
            if ( nursery.isPointer( p ) )
                r << followPointer( p ) << " ";
            else
                r << fmtInteger( where + p.offset, 32 ) << " ";
        }
        if ( i < int( nursery.offsets.size() ) - 1 )
            r << "| ";
    }
    r << std::endl;

    for ( int i = 0; i < _thread_count; ++ i ) {
        int count = 0;
        r << "thread " << i << ", stack depth = " << stack( i ).get().length() << std::endl;
        eachframe( stack( i ), [&]( Frame &f ) {
                r << "frame[" << count << "]: pc = (" << f.pc
                  << "), data = ";
                ++ count;
                if ( f.pc.function >= int( _info.functions.size() ) ) {
                    r << "<invalid PC>" << std::endl;
                    return;
                }
                auto fun = _info.function( f.pc );
                r << "[" << fun.datasize << " bytes] ";
                for ( auto i = fun.values.begin(); i != fun.values.end(); ++ i ) {
                    r << "[" << i->offset << "]";
                    if ( f.isPointer( _info, *i ) )
                        r << *f.dereference< Pointer >( _info, *i ) << " ";
                    else
                        r << fmtInteger( f.dereference( _info, *i ), i->width * 8 ) << " ";
                }
                r << std::endl;
            });
    }

    r << "--------" << std::endl;
}

void MachineState::dump() {
    dump( std::cerr );
}

void ProgramInfo::Instruction::dump( ProgramInfo &info, MachineState &state ) {
    op->dump();
    std::string fl = isa< ::llvm::Instruction >( op ) ? fileline( *cast< ::llvm::Instruction >( op ) ) : "";
    if ( !fl.empty() )
        std::cerr << "  location: " << fl << std::endl;
    for ( int i = 0; i < int( values.size() ); ++i ) {
        ProgramInfo::Value v = values[i];
        if ( !i )
            std::cerr << "  result: ";
        else
            std::cerr << "  operand " << i - 1 << ": ";
        std::cerr << (v.constant ? "constant" : (v.global ? "global" : "local"))
                  << ", type " << v.type << " at " << v.offset << ", width = " << v.width;
        if ( !v.constant ) {
            std::cerr << ", value = " << fmtInteger( state.dereference( v ), v.width * 8 );
            std::cerr << ", pointer = " << state.isPointer( v );
        } else
             std::cerr << ", value = " << fmtInteger( &info.constdata[ v.offset ], v.width * 8 );
        std::cerr << std::endl;
    }
}

std::string Interpreter::describe( bool detailed ) {
    return Describe( this, detailed ).all();
}

std::string Interpreter::describeConstdata() {
    return Describe( this, false ).constdata();
}

