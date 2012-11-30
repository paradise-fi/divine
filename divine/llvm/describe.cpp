// -*- C++ -*- (c) 2011 Petr Rockai <me@mornfall.net>
// Describe the interpreter's state in a human-readable fashion.

#include <divine/llvm/interpreter.h>
#include <llvm/Analysis/DebugInfo.h>
#include <llvm/ADT/StringMap.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;
using namespace divine::llvm;

Interpreter::Describe Interpreter::describeAggregate( Type *t, char *where, DescribeSeen &seen ) {
    char delim[2];
    std::vector< std::string > vec;
    Describe sub;

    if ( isa< StructType >( t ) ) {
        delim[0] = '{'; delim[1] = '}';
        const StructType *stru = cast< StructType >( t );
        for ( Type::subtype_iterator st = stru->element_begin();
              st != stru->element_end(); ++ st ) {
            sub = describeValue( (*st), where, seen );
            vec.push_back( sub.first );
            where = sub.second;
        }
    }

    if ( isa< ArrayType >( t )) {
        delim[0] = '['; delim[1] = ']';
        const ArrayType *arr = cast< ArrayType >( t );
        for ( int i = 0; i < arr->getNumElements(); ++ i ) {
            sub = describeValue( arr->getElementType(), where, seen );
            vec.push_back( sub.first );
            assert_neq( sub.second, where );
            where = sub.second;
        }
    }

    return std::make_pair( wibble::str::fmt_container( vec, delim[0], delim[1] ), where );
}

std::string Interpreter::describePointer( Type *t, Pointer p, DescribeSeen &seen )
{
    if ( p.null() )
        return "null";

    std::string ptr = wibble::str::fmt( p );
    std::string res;
    Type *pointeeTy = cast< PointerType >( t )->getElementType();
    if ( isa< FunctionType >( pointeeTy ) ) {
        res = "@<???>"; // TODO functionIndex.right( idx )->getName().str();
    } else if ( seen.count( std::make_pair( p, pointeeTy ) ) ) {
        res = ptr + " <...>";
    } else {
        if ( state.validate( p ) ) {
            Describe pointee = describeValue( pointeeTy, dereference( p ), seen );
            res = ptr + " " + pointee.first;
        } else
            res = ptr + " (not mapped)";
        seen.insert( std::make_pair( p, pointeeTy ) );
    }
    return res;
}

static std::string fmtInteger( char *where, int bits ) {
    switch ( bits ) {
        case 64: return wibble::str::fmt( *(int64_t*) where);
        case 32: return wibble::str::fmt( *(int32_t*) where);
        case 16: return wibble::str::fmt( *(int16_t*) where);
        case 8: return wibble::str::fmt( int( *(int8_t *) where ) );
        case 1: return wibble::str::fmt( *(bool *) where );
        default: return "<" + wibble::str::fmt( bits ) + "-bit integer>";
    }
}

Interpreter::Describe Interpreter::describeValue( Type *t, char *where, DescribeSeen &seen ) {
    std::string res;
    if ( t->isIntegerTy() ) {
        res = fmtInteger( where, t->getPrimitiveSizeInBits() );
        where += t->getPrimitiveSizeInBits() / 8;
    } else if ( t->isPointerTy() ) {
        res = describePointer( t, *reinterpret_cast< Pointer* >( where ), seen );
        where += sizeof( Pointer );
    } else if ( t->getPrimitiveSizeInBits() ) {
        res = "<weird scalar>";
        where += t->getPrimitiveSizeInBits() / 8;
    } else if ( t->isAggregateType() ) {
        Describe sub = describeAggregate( t, where, seen );
        res = sub.first;
        where = sub.second;
    } else res = "<weird type>";
    return std::make_pair( res, where );
}

std::string Interpreter::describeValue( const ::llvm::Value *val, int thread,
                                        DescribeSeen *seen,
                                        int *anonymous,
                                        std::vector< std::string > *container )
{
    std::string name, value;
    DescribeSeen _seen;
    if ( !seen )
        seen = &_seen;

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
        name = ""; // ignore, but include in anonymous counter

    char *where = state.dereference( ValueRef( info.valuemap[ val ], 0, thread ) );
    if ( type->isPointerTy() )
        value = describePointer( type, *reinterpret_cast< Pointer * >( where ), *seen );
    else
        value = describeValue( type, where, *seen ).first;

    if ( container && !name.empty() )
        container->push_back( name + " = " + value );

    return name.empty() ? "" : name + " = " + value;
}

std::string Interpreter::describe( bool detailed ) {
    std::stringstream s;
    std::vector< std::string > globals;

    DescribeSeen seen;

    for ( auto v = module->global_begin(); v != module->global_end(); ++v )
        describeValue( &*v, 0, &seen, nullptr, &globals );

    if ( globals.size() )
        s << "global: " << wibble::str::fmt( globals ) << std::endl;

    for ( int c = 0; c < state._thread_count; ++c ) {
        std::vector< std::string > vec;
        std::stringstream locs;

        if ( state.stack( c ).get().length() &&
             info.instruction( state.frame( c ).pc ).op )
        {
            const Instruction &insn = cast< const Instruction >( *info.instruction( state.frame( c ).pc ).op );
            const LLVMContext &ctx = insn.getContext();
            const DebugLoc &loc = insn.getDebugLoc();
            const Function *f = insn.getParent()->getParent();
            DILocation des( loc.getAsMDNode( ctx ) );
            locs << "<" << f->getName().str() << ">";
            if ( des.getLineNumber() )
                locs << " [ " << des.getFilename().str() << ":" << des.getLineNumber() << " ]";
            else {
                std::string descr;
                raw_string_ostream descr_stream( descr );
                descr_stream << insn;
                locs << " [" << std::string( descr, 1, std::string::npos ) << " ]";
            }

            int _anonymous = 0;
            int *anonymous = detailed ? &_anonymous : nullptr;

            for ( auto arg = f->arg_begin(); arg != f->arg_end(); ++ arg )
                describeValue( &*arg, c, &seen, anonymous, &vec );

            for ( auto block = f->begin(); block != f->end(); ++block ) {
                describeValue( &*block, c, &seen, anonymous, &vec );
                for ( auto v = block->begin(); v != block->end(); ++v )
                    describeValue( &*v, c, &seen, anonymous, &vec );
            }
        } else
            locs << "<null>";

        s << c << ": " << locs.str() << " " << wibble::str::fmt( vec ) << std::endl;
    }

    if ( !state._thread_count )
        s << "! EXIT" << std::endl;

    if ( state.flags().assert )
        s << "! ASSERTION FAILED" << std::endl;

    if ( state.flags().invalid_argument )
        s << "! INVALID ARGUMENT" << std::endl;

    if ( state.flags().invalid_dereference )
        s << "! INVALID DEREFERENCE" << std::endl;

    if ( state.flags().null_dereference )
        s << "! NULL DEREFERENCE" << std::endl;

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
    Flags &fl = flags();

    r << "flags: [ " << fl.assert << " " << fl.null_dereference << " " << fl.invalid_dereference
      << " " << fl.invalid_argument << " " << fl.ap << " " << fl.buchi << " ]" << std::endl;

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
            if ( heap().isPointer( p ) ) {
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
    for ( int i = 0; i < nursery.offsets.size() - 1; ++ i ) {
        Pointer p( true, i + nursery.segshift, 0 );
        char *where = nursery.dereference( p );
        int size = nursery.size( p );
        for ( ; p.offset < size; p.offset += 4 ) {
            if ( nursery.isPointer( p ) )
                r << followPointer( p ) << " ";
            else
                r << fmtInteger( where + p.offset, 32 ) << " ";
        }
        if ( i < nursery.offsets.size() - 1 )
            r << "| ";
    }
    r << std::endl;

    for ( int i = 0; i < _thread_count; ++ i ) {
        int count = 0;
        r << "thread " << i << ", stack depth = " << stack( i ).get().length() << std::endl;
        eachframe( stack( i ), [&]( Frame &f ) {
                r << "frame[" << count << "]: pc = ("
                  << f.pc.function << ":" << f.pc.block << ":"
                  << f.pc.instruction << "), data = ";
                ++ count;
                if ( f.pc.function >= _info.functions.size() ) {
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
