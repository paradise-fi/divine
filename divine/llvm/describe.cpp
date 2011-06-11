// -*- C++ -*- (c) 2011 Petr Rockai <me@mornfall.net>
// Describe the interpreter's state in a human-readable fashion.

#ifdef HAVE_LLVM

#include <divine/llvm/interpreter.h>
#include "llvm/Analysis/DebugInfo.h"

using namespace llvm;
using namespace divine::llvm;

Interpreter::Describe Interpreter::describeAggregate( const Type *t, void *where, DescribeSeen &seen ) {
    char delim[2];
    std::vector< std::string > vec;
    Describe sub;

    if ( isa< StructType >( t ) ) {
        delim[0] = '{'; delim[1] = '}';
        const StructType *stru = cast< StructType >( t );
        for ( Type::subtype_iterator st = stru->element_begin();
              st != stru->element_end(); ++ st ) {
            sub = describeValue( st->get(), where, seen );
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

std::string Interpreter::describePointer( const Type *t, int idx, DescribeSeen &seen ) {
    std::string res = "*" + wibble::str::fmt( (void*) idx );
    if ( idx ) {
        if ( seen.count( idx ) ) {
            res += " <...>";
        } else {
            Describe pointee = describeValue( cast< PointerType >( t )->getElementType(),
                                              arena.translate( idx ), seen );
            res += " " + pointee.first;
            seen.insert( idx );
        }
    }
    return res;
}

Interpreter::Describe Interpreter::describeValue( const Type *t, void *where, DescribeSeen &seen ) {
    std::string res;
    if ( t->isIntegerTy() ) {
        where += t->getPrimitiveSizeInBits() / 8;
        if ( t->isIntegerTy( 32 ) )
            res = wibble::str::fmt( *(int32_t*) where);
        if ( t->isIntegerTy( 8 ) )
            res = wibble::str::fmt( int( *(int8_t *) where ) );
    } else if ( t->isPointerTy() ) {
        res = describePointer( t, *(intptr_t*) where, seen );
    } else if ( t->getPrimitiveSizeInBits() ) {
        where += t->getPrimitiveSizeInBits() / 8;
        res = "?";
    } else if ( t->isAggregateType() ) {
        Describe sub = describeAggregate( t, where, seen );
        where = sub.second;
        res = sub.first;
    }
    return std::make_pair( res, where );
}

std::string Interpreter::describeGenericValue( int vindex, GenericValue vvalue, DescribeSeen *seen ) {
    std::string str;
    DescribeSeen _seen;
    if ( !seen )
        seen = &_seen;
    Value *val = valueIndex.right( vindex );
    const Type *type = val->getType();
    if ( val->getValueName() ) {
        str = std::string( val->getValueName()->getKey() ) + " = ";
        if ( type->isPointerTy() ) {
            str += describePointer( type, intptr_t( vvalue.PointerVal ), *seen );
        } else { // assume intval for now
            str += vvalue.IntVal.toString( 10, 1 );
        }
    }
    return str;
}

std::string Interpreter::describe() {
    std::stringstream s;
    DescribeSeen seen;
    for ( int c = 0; c < stacks.size(); ++c ) {
        const Instruction &insn =
            *locationIndex.right( stack( c ).back().pc ).insn;
        std::vector< std::string > vec;

        const LLVMContext &ctx = insn.getContext();
        const DebugLoc &loc = insn.getDebugLoc();
        DILocation des( loc.getAsMDNode( ctx ) );
        if ( des.getLineNumber() )
            vec.push_back( std::string( des.getFilename() ) +
                           ":" + wibble::str::fmt( des.getLineNumber() ) );
        else
            vec.push_back( "<unknown>" );

        for ( ExecutionContext::Values::iterator v = stack( c ).back().values.begin();
              v != stack( c ).back().values.end(); ++v ) {
            std::string vdes = describeGenericValue( v->first, v->second, &seen );
            if ( !vdes.empty() )
                vec.push_back( vdes );
        }
        s << wibble::str::fmt( vec ) << std::endl;
    }
    return s.str();
}

#endif
