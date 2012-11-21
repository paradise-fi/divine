// -*- C++ -*- (c) 2011 Petr Rockai <me@mornfall.net>
// Describe the interpreter's state in a human-readable fashion.

#include <divine/llvm-new/interpreter.h>
#include "llvm/Analysis/DebugInfo.h"

using namespace llvm;
using namespace divine::llvm2;

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
    if ( !p.valid )
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
            Describe pointee = describeValue( pointeeTy, dereferencePointer( p ), seen );
            res = ptr + " " + pointee.first;
        } else
            res = ptr + " (not mapped)";
        seen.insert( std::make_pair( p, pointeeTy ) );
    }
    return res;
}

Interpreter::Describe Interpreter::describeValue( Type *t, char *where, DescribeSeen &seen ) {
    std::string res;
    if ( t->isIntegerTy() ) {
        if ( t->isIntegerTy( 64 ) )
            res = wibble::str::fmt( *(int64_t*) where);
        if ( t->isIntegerTy( 32 ) )
            res = wibble::str::fmt( *(int32_t*) where);
        if ( t->isIntegerTy( 8 ) )
            res = wibble::str::fmt( int( *(int8_t *) where ) );
        where += t->getPrimitiveSizeInBits() / 8;
    } else if ( t->isPointerTy() ) {
        res = describePointer( t, *reinterpret_cast< Pointer* >( where ), seen );
        where += sizeof( Pointer );
    } else if ( t->getPrimitiveSizeInBits() ) {
        res = "?";
        where += t->getPrimitiveSizeInBits() / 8;
    } else if ( t->isAggregateType() ) {
        Describe sub = describeAggregate( t, where, seen );
        res = sub.first;
        where = sub.second;
    } else res = "?";
    return std::make_pair( res, where );
}

std::string Interpreter::describeValue( ProgramInfo::Value v, int thread, DescribeSeen *seen ) {
    std::string str, name;
    DescribeSeen _seen;
    if ( !seen )
        seen = &_seen;

    Type *type = val->getType();
    auto vname = val->getValueName();

    if ( vname )
        name = vname->getKey();
    if ( name.empty() )
        return "";
    str = name + " = ";
    char *where = state.dereference( v, thread );
    if ( type->isPointerTy() )
        str += describePointer( type, *reinterpret_cast< Pointer * >( where ), *seen );
    else
        str += describeValue( type, where, *seen ).first;
    return str;
}

std::string Interpreter::describe() {
    std::stringstream s;
    DescribeSeen seen;
    for ( int c = 0; c < state._thread_count; ++c ) {
        std::vector< std::string > vec;
        std::stringstream locs;

        if ( state.stack( c ).get().length() &&
             info.instruction( state.frame( c ).pc ).op )
        {
            const Instruction &insn = *info.instruction( state.frame( c ).pc ).op;
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
            auto function = info.function( state.frame( c ).pc );
            for ( auto v = function.values.begin(); v != function.values.end(); ++v )
            {
                std::string vdes = describeValue( *v, c, &seen );
                if ( !vdes.empty() )
                    vec.push_back( vdes );
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
