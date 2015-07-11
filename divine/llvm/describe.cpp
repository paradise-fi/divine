// -*- C++ -*- (c) 2011-2014 Petr Rockai <me@mornfall.net>
// Describe the interpreter's state in a human-readable fashion.

#include <brick-string.h>
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
#include <llvm/Assembly/Writer.h>

#include <cxxabi.h>
#include <cstdlib>
#include <iomanip>

#pragma GCC diagnostic pop

using namespace llvm;
using namespace divine::llvm;

namespace divine {
namespace llvm {

template< typename HM, typename L >
struct Describe {

    typedef std::set< std::pair< Pointer, Type * > > DescribeSeen;

    DescribeSeen seen;
    std::vector< std::string > lines;
    bool detailed;
    Interpreter< HM, L > *interpreter;
    bool _demangle;

    std::string pointer( Type *t, Pointer p );
    Describe( Interpreter< HM, L > *i, bool demangle, bool detailed )
        : detailed( detailed ), interpreter( i ), _demangle( demangle )
    {}

    std::string all();
    std::string constdata();

    using HeapMeta = HM;

    ::llvm::TargetData &TD() { return interpreter->TD; }
    MachineState< HM > &state() { return interpreter->state; }
    ProgramInfo &info() { return interpreter->info(); }

    template< typename Ptr > std::string aggregate( Type *t, Ptr where );

    template< typename Ptr > std::string value( Type *t, Ptr where );
    template< typename Ptr >
    std::string value( const ::llvm::Value *, ValueRef vref, Ptr p );
    template< typename Ptr >
    std::string value( std::pair< ::llvm::Type *, std::string >, ValueRef vref, Ptr p );

    std::string problem( Problem bad );
    std::string locinfo( PC pc, bool instruction = false,
                         Function **fun = nullptr );

    bool boring( std::string n, bool fun = false ) {
        if ( n.empty() )
            return true;
        if ( !detailed && n.find( '.' ) != std::string::npos )
            return true;
        if ( n.length() >= 2 && n[0] == '_' && std::isupper( n[1] ) )
            return true;
        if ( fun && ( brick::string::startsWith( n, "pthread_" ) ) )
             return true;
        return false;
    }

    std::string demangle( std::string mangled )
    {
        if ( _demangle ) {
            int stat;
            auto x = abi::__cxa_demangle( mangled.c_str(), nullptr, nullptr, &stat );
            auto ret = stat == 0 && x ? std::string( x ) : mangled;
            std::free( x );
            return ret;
        }
        return mangled;
    }

};

std::ostream &operator<<( std::ostream &o, PC p ) {
    return o << p.function << ":" << p.instruction;
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

template< typename HM, typename L > template< typename Ptr >
std::string Describe< HM, L >::aggregate( Type *t, Ptr where )
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
            where.offset = startoffset + SLO->getElementOffset( index );
            vec.push_back( value( (*st), where ) );
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

    return brick::string::fmt_container( vec, delim[0], delim[1] );
}

template< typename HM, typename L >
std::string Describe< HM, L >::pointer( Type *t, Pointer p )
{
    if ( p.null() )
        return "null";

    std::string ptr = brick::string::fmt( p );
    std::string res;
    Type *pointeeTy = cast< PointerType >( t )->getElementType();
    if ( p.code ) {
        res = brick::string::fmt( static_cast< PC >( p ) );
    } else if ( seen.count( std::make_pair( p, pointeeTy ) ) ) {
        res = ptr + " <...>";
    } else {
        seen.insert( std::make_pair( p, pointeeTy ) );
        if ( state().validate( p ) && state().inBounds( p, 0 ) )
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
        case 64: return brick::string::fmt( *reinterpret_cast< int64_t* >( where ) );
        case 32: return brick::string::fmt( *reinterpret_cast< int32_t* >( where ) );
        case 16: return brick::string::fmt( *reinterpret_cast< int16_t* >( where ) );
        case 8: return brick::string::fmt( int( *reinterpret_cast< int8_t * >( where ) ) );
        case 1: return brick::string::fmt( *reinterpret_cast< bool * >( where ) );
        default:
            std::string rv = "< ";
            for ( int i = 0; i < bits / 8; i += 4 )
                rv += fmtInteger( where + i, 32 ) + " ";
            return rv + ">";
    }
}

void updateWidth( ::llvm::TargetData TD, ValueRef &w, Type *t ) {
    w.v.width = TD.getTypeAllocSize( t );
}

void updateWidth( ::llvm::TargetData, Pointer, Type * ) {}

template< typename HM, typename L > template< typename Ptr >
std::string Describe< HM, L >::value( Type *t, Ptr where )
{
    updateWidth( TD(), where, t );
    if ( t->isAggregateType() )
        return aggregate( t, where );
    if ( t->isPointerTy() ) {
        char *mem = interpreter->dereference( where );
        return pointer( t, mem ? *reinterpret_cast< Pointer * >( mem ) : Pointer() );
    }
    if ( t->isIntegerTy() ) {
        int w = TD().getTypeAllocSize( t ) * 8;
        auto mflag = state().memoryflag( where );
        bool initd = true;
        if ( mflag.valid() )
            for ( int i = 0; i < w; ++i, ++mflag )
                if ( mflag.get() == MemoryFlag::Uninitialised )
                    initd = false;
        return fmtInteger( state().dereference( where ), w ) + (initd ? "" : "?");
    }
    if ( t->getPrimitiveSizeInBits() )
        return "<weird scalar>";
    return "<weird type>";
}

template< typename HM, typename L > template< typename Ptr >
std::string Describe< HM, L >::value( const ::llvm::Value *val, ValueRef vref, Ptr where )
{
    std::string name, value;

    Type *type = val->getType();
    auto vname = val->getValueName();

    if ( vname ) {
        name = vname->getKey().str();
    } else if ( type->isVoidTy() )
        ;
    else if ( detailed ) {
        if ( info().anonmap.find( val ) == info().anonmap.end() ) {
            ::llvm::raw_string_ostream name_s( name );
            ::llvm::WriteAsOperand(name_s, val, false, info().module );
            name_s.flush();
            info().anonmap[ val ] = name;
        } else
            name = info().anonmap[ val ];
    }

    if ( boring( name ) || isa< BasicBlock >( val ) )
        return "";

    return this->value( std::make_pair( type, name ), vref, where );
}

template< typename HM, typename L > template< typename Ptr >
std::string Describe< HM, L >::value( std::pair< ::llvm::Type *, std::string > val,
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
        auto mflag = state().memoryflag( vref );
        if ( type->isPointerTy() && mflag.valid() && mflag.get() == MemoryFlag::HeapPointer ) {
            char *mem = state().dereference( vref );
            value = pointer( type, *reinterpret_cast< Pointer * >( mem ) );
        } else if ( vref.v.type == ProgramInfo::Value::Aggregate )
            value = aggregate( type, vref );
        else {
            bool initd = true;
            if ( vref.v.width && state().dereference( vref ) )
                for ( int i = 0; i < vref.v.width; ++i, ++mflag )
                    if ( mflag.valid() && mflag.get() == MemoryFlag::Uninitialised )
                        initd = false;
            value = fmtInteger( state().dereference( vref ), vref.v.width * 8 ) + (initd ? "" : "?");
        }
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
               brick::string::fmt( des.getLineNumber() );
    return "";
}

template< typename HM, typename L >
std::string Describe< HM, L >::locinfo( PC pc, bool instruction,
                                     Function **fun )
{
    auto user = info().instruction( pc ).op;
    if ( !isa< ::llvm::Instruction >( user ) )
        return "<non-code location>";

    std::stringstream locs;
    Instruction &insn = *cast< Instruction >( user );
    Function *f = insn.getParent()->getParent();
    std::string fl = fileline( insn );
    locs << "<" << demangle( f->getName().str() ) << ">";

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

template< typename HM, typename L >
std::string Describe< HM, L >::problem( Problem bad )
{
    std::stringstream s;
    switch ( bad.what ) {
        case Problem::Other:
            s << "PROBLEM"; break;
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
        case Problem::UnreachableExecuted:
            s << "UNREACHABLE EXECUTED"; break;
        case Problem::MemoryLeak:
            s << "MEMORY LEAK"; break;
        case Problem::NotImplemented:
            s << "NOT IMPLEMENTED"; break;
        case Problem::Uninitialised:
            s << "UNDEFINED VALUE"; break;
        case Problem::PointsToViolated:
            s << "POINTS-TO VIOLATED"; break;
        case Problem::Deadlock:
            s << "PTHREAD DEADLOCK"; break;
        default:
            s << "????"; break;
    }

    if ( bad.where.function ) {
        s << " (thread " << int( bad.tid ) << "): ";
        s << locinfo( bad.where, !info().instruction( bad.where ).builtin );
    }

    if ( state().validate( bad.pointer ) ) {
        std::vector< char > str;
        char *ptr = state().dereference( bad.pointer );
        for ( int i = 0; state().inBounds( bad.pointer, i ); ++i )
            str.push_back( ptr[i] );

        s << ": (" << bad.pointer << ")" << ": \""
          << std::string( str.begin(), str.end() ) << "\"";
        for ( int i = 0; i < int( str.size() ); ++ i ) {
            if ( i % 32 == 0 )
                s << "\n    ";
            s << std::setbase( 16 )
              << std::setw( 2 ) << std::setfill( '0' )
              << unsigned( uint8_t( str[i] ) ) << " ";
        }
    } else if ( !bad.pointer.null() || bad.what == Problem::InvalidDereference ) {
        s << ": the offending pointer was " << bad.pointer;
    }

    return s.str();
}

template< typename HM, typename L >
std::string Describe< HM, L >::all()
{
    std::stringstream s;

    for ( int i = 0; i < int( info().globalinfo.size() ); ++ i )
        if ( info().globalinfo[ i ].first )
            this->value( info().globalinfo[ i ], ValueRef(), Pointer( false, i, 0 ) );

    if ( lines.size() ) {
        s << "globals: " << std::endl;
        for ( auto l : lines )
            s << "  " << l << std::endl;
    }

    for ( int c = 0; c < state()._thread_count; ++c ) {
        std::string location;

        int fcount = state().stack( c ).get().length();
        s << "thread " << c << ":" << (fcount ? "" : " (zombie)") << std::endl;
        int i = 0;

        state().eachframe( state().stack( c ), [&]( machine::Frame &f ) {
                ++ i;
                location = "<unknown>";
                this->lines.clear();
                if ( this->info().instruction( f.pc ).op )
                {
                    Function *fun = nullptr;
                    location = this->locinfo( f.pc, false, &fun );

                    if ( fun ) {
                        for ( auto arg = fun->arg_begin(); arg != fun->arg_end(); ++ arg )
                            this->value( &*arg, ValueRef( this->info().valuemap[ &*arg ], fcount - i, c ), Pointer() );

                        for ( auto block = fun->begin(); block != fun->end(); ++block ) {
                            this->value( &*block, ValueRef(), Pointer() ); // just for counting
                            for ( auto v = block->begin(); v != block->end(); ++v )
                                this->value( &*v, ValueRef( this->info().valuemap[ &*v ], fcount - i, c), Pointer() );
                        }
                    }
                }
                s << "  #" << i << ": " << location << " " << brick::string::fmt( this->lines ) << std::endl;
                return true; // continue
            } );
    }

    machine::Flags &flags = state().flags();
    for ( int i = 0; i < flags.problemcount; ++i )
        s << problem( flags.problems(i) ) << std::endl;

    if ( !state()._thread_count )
        s << "EXIT" << std::endl;

    if ( flags.ap ) {
        int ap = flags.ap;
        std::vector< std::string > x;
        int k = 0;
        MDNode *apmeta = interpreter->findEnum( "APs" );
        if ( apmeta ) {
            while ( ap ) {
                if ( ap % 2 ) {
                    MDString *name = cast< MDString >(
                        cast< MDNode >( apmeta->getOperand( k ) )->getOperand( 1 ) );
                    x.push_back( name->getString() );
                }
                ap = ap >> 1;
                ++k;
            }
            s << "+ APs: " << brick::string::fmt( x ) << std::endl;
        } else
            s << "+ APs: 0x" << std::hex << ap << std::dec << std::endl;
    }

    return s.str();
}

template< typename HM, typename L >
std::string Describe< HM, L >::constdata() {
    lines.clear();

    for ( int i = 0; i < int( info().constinfo.size() ); ++ i )
        if ( info().constinfo[ i ].first )
            value( info().constinfo[ i ], ValueRef(), Pointer( false, i, 0 ) );

    std::stringstream str;
    for ( auto l : lines )
        str << l << std::endl;
    return str.str();
}

template< typename HeapMeta >
void MachineState< HeapMeta >::dump( std::ostream &r ) {

    /* TODO problem/flag stuff */

    r << "globals: [ ";
    for ( auto v = _info.globals.begin(); v != _info.globals.end(); v ++ ) {
        if ( v->constant )
            continue;
        if ( v->pointer() )
            r << followPointer( *v ) << " ";
        else
            r << fmtInteger( dereference( *v ), v->width * 8 ) << " ";
    }
    r << "]" << std::endl;

    r << "heap: segcount = " << heap().segcount << ", size = " << heap().size() << ", data:" << std::endl;
    for ( int i = 0; i < heap().segcount; ++ i ) {
        Pointer p = Pointer( true, i, 0 );
        char *where = heap().dereference( Pointer( true, i, 0 ) );
        int size = heap().size( p );
        auto mflag = heap().memoryflag( p );
        r << "    " << p << " size " << pointerSize( p ) << ": ";
        for ( ; p.offset < size; p.offset += 4 ) {
            if ( validate( p ) && heap().memoryflag( p ).get() == MemoryFlag::HeapPointer ) {
                r << followPointer( p ) << " ";
            } else
                r << fmtInteger( where + p.offset, 32 ) << " ";
        }
        r << ", flags = [ ";
        for ( int x = 0; x < size; ++x, ++mflag )
            r << static_cast< int >( mflag.get() );
        r << " ]";
        if ( i < heap().segcount - 1 )
            r << std::endl;
    }
    r << std::endl;

    r << "nursery: segcount = " << nursery.offsets.size() - 1
      << ", size = " << nursery.offsets[ nursery.offsets.size() - 1 ]
      << ", data = ";
    for ( int i = 0; i < int( nursery.offsets.size() ) - 1; ++ i ) {
        Pointer p( true, i + nursery.segshift, 0 );
        char *where = nursery.dereference( p );
        int size = nursery.size( p );
        auto mflag = nursery.memoryflag( p );
        r << "    " << p << " size " << pointerSize( p ) << ": ";
        for ( ; p.offset < size; p.offset += 4 ) {
            if ( validate( p ) && nursery.memoryflag( p ).get() == MemoryFlag::HeapPointer )
                r << followPointer( p ) << " ";
            else
                r << fmtInteger( where + p.offset, 32 ) << " ";
        }
        r << ", flags = [ ";
        for ( int x = 0; x < size; ++x, ++mflag )
            r << static_cast< int >( mflag.get() );
        r << " ]";
        if ( i < int( nursery.offsets.size() ) - 1 )
            r << std::endl;
    }
    r << std::endl;

    for ( int i = 0; i < _thread_count; ++ i ) {
        int count = 0;
        r << "thread " << i << ", stack depth = " << stack( i ).get().length();
        if ( int( _stack.size() ) > i && _stack[i].first) {
            r << " [detached at " << static_cast< void * >( _pool.dereference( _stack[i].second ) ) << "]";
        }
        r << std::endl;
        eachframe( stack( i ), [&]( Frame &f ) {
                r << "frame[" << count << "]: pc = (" << f.pc
                  << "), data = ";
                ++ count;
                if ( f.pc.function >= int( this->_info.functions.size() ) ) {
                    r << "<invalid PC>" << std::endl;
                    return true;
                }
                auto fun = this->_info.function( f.pc );
                r << "[" << fun.datasize << " bytes]\n";
                for ( auto i = fun.values.begin(); i != fun.values.end(); ++ i ) {
                    r << "          " << *i << " offset " << i->offset << ": ";
                    if ( f.memoryflag( this->_info, *i ).get() == MemoryFlag::HeapPointer )
                        r << *f.dereference< Pointer >( this->_info, *i ) << " ";
                    else
                        r << fmtInteger( f.dereference( this->_info, *i ), i->width * 8 ) << " ";
                    r << ", flags = [ ";
                    auto mflag = f.memoryflag( this->_info, *i );
                    for ( int x = 0; x < i->width; ++x, ++mflag )
                        r << static_cast< int >( mflag.get() );
                    r << " ]\n";
                }
                r << std::endl;
                return true; // continue
            });
    }

    r << "heapmeta:";
    for ( int i = 0; i < heap().segcount; ++i )
        r << " " << state().get( HeapMeta() ).idAt( i );
    r << std::endl;

    r << "--------" << std::endl;
}

template< typename HeapMeta >
void MachineState< HeapMeta >::dump() {
    dump( std::cerr );
}

/*
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
        if ( !v.constant )
            std::cerr << ", value = " << fmtInteger( state.dereference( v ), v.width * 8 );
        else
             std::cerr << ", value = " << fmtInteger( &info.constdata[ v.offset ], v.width * 8 );
        std::cerr << ", flag = " << int( state.memoryflag( v ).get() );
        std::cerr << std::endl;
    }
}
*/

template< typename HM, typename L >
std::string Interpreter< HM, L >::describe( bool demangle, bool detailed ) {
    return Describe< HM, L >( this, demangle, detailed ).all();
}

template< typename HM, typename L >
std::string Interpreter< HM, L >::describeConstdata() {
    return Describe< HM, L >( this, false, false ).constdata();
}

template< typename HM, typename L >
void Interpreter< HM, L >::dump() {
    state.dump();
    std::cerr << describe( false, true ) << std::endl;
}

namespace divine {
namespace llvm {

/* explicit instances */
template struct Interpreter< machine::NoHeapMeta, graph::NoLabel >;
template struct Interpreter< machine::NoHeapMeta, graph::ControlLabel >;
template struct Interpreter< machine::NoHeapMeta, graph::Probability >;
template struct Interpreter< machine::HeapIDs, graph::NoLabel >;

}
}
