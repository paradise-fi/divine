// -*- C++ -*- (c) 2011 Petr Rockai <me@mornfall.net>
// Describe the interpreter's state in a human-readable fashion.

#include <divine/llvm/interpreter.h>
#include <llvm/Config/config.h>
#if ( LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR < 2 )
  #include <llvm/Analysis/DebugInfo.h>
#else
  #include <llvm/DebugInfo.h>
#endif
#include <llvm/ADT/StringMap.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;
using namespace divine::llvm;

namespace divine {
namespace llvm {

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
std::string Interpreter::describeAggregate( Type *t, Ptr where, DescribeSeen &seen )
{
    char delim[2];
    std::vector< std::string > vec;

    if ( isa< StructType >( t ) ) {
        delim[0] = '{'; delim[1] = '}';
        const StructType *stru = cast< StructType >( t );
        for ( auto st = stru->element_begin(); st != stru->element_end(); ++ st )
        {
            vec.push_back( describeValue( (*st), where, seen ) );
            where.offset += TD.getTypeAllocSize( *st );
        }
    }

    if ( isa< ArrayType >( t )) {
        delim[0] = '['; delim[1] = ']';
        const ArrayType *arr = cast< ArrayType >( t );
        for ( int i = 0; i < int( arr->getNumElements() ); ++ i )
        {
            vec.push_back( describeValue( arr->getElementType(), where, seen ) );
            where.offset += TD.getTypeAllocSize( arr->getElementType() );
        }
    }

    return wibble::str::fmt_container( vec, delim[0], delim[1] );
}

std::string Interpreter::describePointer( Type *t, Pointer p, DescribeSeen &seen )
{
    if ( p.null() )
        return "null";

    std::string ptr = wibble::str::fmt( p );
    std::string res;
    Type *pointeeTy = cast< PointerType >( t )->getElementType();
    if ( isa< FunctionType >( pointeeTy ) ) {
        res = "@<some function>"; // TODO functionIndex.right( idx )->getName().str();
    } else if ( seen.count( std::make_pair( p, pointeeTy ) ) ) {
        res = ptr + " <...>";
    } else {
        seen.insert( std::make_pair( p, pointeeTy ) );
        if ( state.validate( p ) )
            res = "@(" + ptr + "| " + describeValue( pointeeTy, p, seen ) + ")";
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
std::string Interpreter::describeValue( Type *t, Ptr where, DescribeSeen &seen )
{
    updateWidth( TD, where, t );
    if ( t->isAggregateType() )
        return describeAggregate( t, where, seen );
    if ( t->isPointerTy() ) {
        if ( state.isPointer( where ) )
            return describePointer( t, state.followPointer( where ), seen );
        else
            return describePointer( t, Pointer(), seen );
    }
    if ( t->isIntegerTy() )
        return fmtInteger( state.dereference( where ), TD.getTypeAllocSize( t ) * 8 );
    if ( t->getPrimitiveSizeInBits() )
        return "<weird scalar>";
    return "<weird type>";
}

template< typename Ptr >
std::string Interpreter::describeValue( const ::llvm::Value *val, ValueRef vref, Ptr where,
                                        DescribeSeen &seen, int *anonymous,
                                        std::vector< std::string > *container )
{
    std::string name, value;

    Type *type = val->getType();
    auto vname = val->getValueName();

    if ( vname ) {
        name = vname->getKey();
        if ( name.find( '.' ) != std::string::npos && !anonymous )
            name = ""; /* ignore compiler-generated values */
    } else if ( type->isVoidTy() )
        ;
    else if ( anonymous ) /* This is a hack. */
        name = "%" + wibble::str::fmt( (*anonymous)++ );

    if ( isa< BasicBlock >( val ) )
        return "";

    return describeValue( std::make_pair( type, name ), vref, where, seen, anonymous, container );
}

template< typename Ptr >
std::string Interpreter::describeValue( std::pair< ::llvm::Type *, std::string > val, ValueRef vref,
                                        Ptr where, DescribeSeen &seen, int *anonymous,
                                        std::vector< std::string > *container )
{
    if ( where.null() && !vref.v.width )
        return "";

    ::llvm::Type *type = val.first;
    std::string name = val.second;
    std::string value;

    if ( !where.null() ) {
        value = describeValue( type, where, seen );
    } else { /* scalar */
        if ( type->isPointerTy() && state.isPointer( vref ) ) {
            char *mem = state.dereference( vref );
            value = describePointer( type, *reinterpret_cast< Pointer * >( mem ), seen );
        } else if ( vref.v.type == ProgramInfo::Value::Aggregate )
            value = describeAggregate( type, vref, seen );
        else
            value = fmtInteger( state.dereference( vref ), vref.v.width * 8 );
    }

    if ( container && !name.empty() )
        container->push_back( name + " = " + value );

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
    }
    s << " (thread " << int( bad.tid ) << "): ";
    s << locinfo( info, bad.where, bad.what != Problem::Assert );
    return s.str();
}

std::string Interpreter::describe( bool detailed ) {
    std::stringstream s;
    std::vector< std::string > globals;

    DescribeSeen seen;

    for ( auto i = info().globalinfo.begin(); i != info().globalinfo.end(); ++i )
        describeValue( i->second, ValueRef(), i->first, seen, nullptr, &globals );

    if ( globals.size() )
        s << "global: " << wibble::str::fmt( globals ) << std::endl;

    for ( int c = 0; c < state._thread_count; ++c ) {
        std::vector< std::string > vec;
        std::string location;

        if ( state.stack( c ).get().length() &&
             info().instruction( state.frame( c ).pc ).op )
        {
            Function *f = nullptr;
            location = locinfo( info(), state.frame( c ).pc, false, &f );

            if ( !f )
                break;

            int _anonymous = 0;
            int *anonymous = detailed ? &_anonymous : nullptr;

            for ( auto arg = f->arg_begin(); arg != f->arg_end(); ++ arg )
                describeValue( &*arg, ValueRef( info().valuemap[ &*arg ], 0, c ), Pointer(),
                               seen, anonymous, &vec );

            for ( auto block = f->begin(); block != f->end(); ++block ) {
                describeValue( &*block, ValueRef(), Pointer(), seen, anonymous, &vec ); // just for counting
                for ( auto v = block->begin(); v != block->end(); ++v )
                    describeValue( &*v, ValueRef( info().valuemap[ &*v ], 0, c), Pointer(),
                                   seen, anonymous, &vec );
            }
        } else
            location = "<null>";

        s << c << ": " << location << " " << wibble::str::fmt( vec ) << std::endl;
    }

    MachineState::Flags &flags = state.flags();
    for ( int i = 0; i < flags.problemcount; ++i )
        s << describeProblem( info(), flags.problems(i) ) << std::endl;

    if ( !state._thread_count )
        s << "EXIT" << std::endl;

#if 0
    if ( flags.ap ) {
        int ap = flags.ap;
        std::vector< std::string > x;
        int k = 0;
        MDNode *apmeta = findEnum( "AP" );
        while ( ap ) {
            if ( ap % 2 ) {
                MDString *name = cast< MDString >(
                    cast< MDNode >( apmeta->getOperand( k ) )->getOperand( 1 ) );
                x.push_back( name->getString() );
            }
            ap = ap >> 1;
            ++k;
        }
        s << "+ APs: " << wibble::str::fmt( x );
    }
#endif

    return s.str();
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
