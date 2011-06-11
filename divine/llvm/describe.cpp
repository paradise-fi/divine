// -*- C++ -*- (c) 2011 Petr Rockai <me@mornfall.net>
// Describe the interpreter's state in a human-readable fashion.

#ifdef HAVE_LLVM

#include <divine/llvm/interpreter.h>
#include "llvm/Analysis/DebugInfo.h"

using namespace llvm;
using namespace divine::llvm;

Interpreter::Describe Interpreter::describeAggregate( const Type *t, void *where ) {
    char delim[2];
    std::vector< std::string > vec;
    Describe sub;

    if ( isa< StructType >( t ) ) {
        delim[0] = '{'; delim[1] = '}';
        const StructType *stru = cast< StructType >( t );
        for ( Type::subtype_iterator st = stru->element_begin();
              st != stru->element_end(); ++ st ) {
            sub = describeValue( st->get(), where );
            vec.push_back( sub.first );
            where = sub.second;
        }
    }

    if ( isa< ArrayType >( t )) {
        delim[0] = '['; delim[1] = ']';
        const ArrayType *arr = cast< ArrayType >( t );
        for ( int i = 0; i < arr->getNumElements(); ++ i ) {
            sub = describeValue( arr->getElementType(), where );
            vec.push_back( sub.first );
            where = sub.second;
        }
    }

    return std::make_pair( wibble::str::fmt_container( vec, delim[0], delim[1] ), where );
}

Interpreter::Describe Interpreter::describeValue( const Type *t, void *where ) {
    std::string res;
    if ( t->isIntegerTy() ) {
        where += t->getPrimitiveSizeInBits() / 8;
        if ( t->isIntegerTy( 32 ) )
            res = wibble::str::fmt( *(int32_t*) where);
        if ( t->isIntegerTy( 8 ) )
            res = wibble::str::fmt( int( *(int8_t *) where ) );
    } else if ( t->isPointerTy() ) {
        where += t->getPrimitiveSizeInBits() / 8;
        res = "*" + wibble::str::fmt( *(void **) where );
    } else if ( t->getPrimitiveSizeInBits() ) {
        where += t->getPrimitiveSizeInBits() / 8;
        res = "?";
    } else if ( t->isAggregateType() ) {
        Describe sub = describeAggregate( t, where );
        where = sub.second;
        res = sub.first;
    }
    return std::make_pair( res, where );
}

std::string Interpreter::describeGenericValue( int vindex, GenericValue vvalue ) {
    std::string str;
    Value *val = valueIndex.right( vindex );
    const Type *type = val->getType();
    if ( val->getValueName() ) {
        str = std::string( val->getValueName()->getKey() ) + " = ";
        if ( type->isPointerTy() ) {
            str += "*" + wibble::str::fmt( vvalue.PointerVal );
            Arena::Index idx = intptr_t( GVTOP( vvalue ) );
            void *where = arena.translate( idx );

            for ( Type::subtype_iterator st = type->subtype_begin();
                  st != type->subtype_end(); ++ st ) {
                Describe pointee = describeValue( st->get(), where );
                where = pointee.second;
                str += " " + pointee.first;
            }
        } else { // assume intval for now
            str += vvalue.IntVal.toString( 10, 1 );
        }
    }
    return str;
}

std::string Interpreter::describe() {
    std::stringstream s;
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
            std::string vdes = describeGenericValue( v->first, v->second );
            if ( !vdes.empty() )
                vec.push_back( vdes );
        }
        s << wibble::str::fmt( vec ) << std::endl;
    }
    return s.str();
}

#endif
