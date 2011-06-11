// -*- C++ -*- (c) 2011 Petr Rockai <me@mornfall.net>
// Describe the interpreter's state in a human-readable fashion.

#ifdef HAVE_LLVM

#include <divine/llvm/interpreter.h>
#include "llvm/Analysis/DebugInfo.h"

using namespace llvm;
using namespace divine::llvm;

std::pair< std::string, int > Interpreter::describeAggregate( const Type *t, void *where ) {
    int offset = 0;
    char delim[2];
    std::vector< std::string > vec;
    if ( isa< StructType >( t ) ) {
        delim[0] = '{'; delim[1] = '}';
        const StructType *stru = cast< StructType >( t );
        for ( Type::subtype_iterator st = stru->element_begin();
              st != stru->element_end(); ++ st ) {
            if ( st->get()->isIntegerTy( 32 ) ) {
                vec.push_back( wibble::str::fmt( *(int32_t*) (where + offset)) );
                offset += 4;
            } else if ( st->get()->getPrimitiveSizeInBits() ) {
                offset += st->get()->getPrimitiveSizeInBits() / 8;
            } else if ( st->get()->isAggregateType() ) {
                std::pair< std::string, int > sub = describeAggregate( st->get(), where + offset );
                vec.push_back( sub.first );
                offset += sub.second;
            } else
                break; // we have lost the offset here
        }
    }
    if ( isa< ArrayType >( t )) {
        delim[0] = '['; delim[1] = ']';
        const ArrayType *arr = cast< ArrayType >( t );
        for ( int i = 0; i < arr->getNumElements(); ++ i ) {
            if ( arr->getElementType()->isIntegerTy( 32 ) ) {
                vec.push_back( wibble::str::fmt( *(int32_t*) (where + offset)) );
                offset += 4;
            }
        }
    }
    return std::make_pair( wibble::str::fmt_container( vec, delim[0], delim[1] ), offset );
}

std::string Interpreter::describeValue( int vindex, GenericValue vvalue ) {
    std::string str;
    Value *val = valueIndex.right( vindex );
    const Type *type = val->getType();
    if ( val->getValueName() ) {
        str = std::string( val->getValueName()->getKey() ) + " = ";
        if ( type->isPointerTy() ) {
            str += "*" + wibble::str::fmt( vvalue.PointerVal );
            Arena::Index idx = intptr_t( GVTOP( vvalue ) );
            for ( Type::subtype_iterator st = type->subtype_begin();
                  st != type->subtype_end(); ++ st ) {
                if ( st->get()->isIntegerTy( 32 ) )
                    str += " (" + wibble::str::fmt( *(int32_t*)arena.translate( idx ) ) + ")";
                if ( st->get()->isPointerTy() )
                    str += " (*" + wibble::str::fmt( *(void **)arena.translate( idx ) ) + ")";
                if ( st->get()->isAggregateType() ) {
                    str += " " + describeAggregate( st->get(), arena.translate( idx ) ).first;
                }
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
            std::string vdes = describeValue( v->first, v->second );
            if ( !vdes.empty() )
                vec.push_back( vdes );
        }
        s << wibble::str::fmt( vec ) << std::endl;
    }
    return s.str();
}

#endif
