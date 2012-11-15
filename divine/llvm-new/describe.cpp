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

#if 0
std::string Interpreter::describePointer( Type *t, int idx, DescribeSeen &seen ) {
    std::string ptr = "*" + wibble::str::fmt( (void*) idx );
    std::string res;
    if ( idx ) {
        Type *pointeeTy = cast< PointerType >( t )->getElementType();
        if ( isa< FunctionType >( pointeeTy ) ) {
            res = "@" + functionIndex.right( idx )->getName().str();
        } else if ( seen.count( std::make_pair( idx, pointeeTy ) ) ) {
            res = ptr + " <...>";
        } else {
            char *ptrAdr = NULL;
            int __idx;
            if ((__idx = idx - 0x100) < constGlobalmem.size()) {
                ptrAdr = &constGlobalmem[__idx];
            } else  if ((__idx -= constGlobalmem.size()) < globalmem.size()) {
                ptrAdr = &globalmem[__idx];
            } else {
                if ( arena.validate(idx) && ((Arena::Index) idx ).block ) {
                    ptrAdr = static_cast< char * >(arena.translate(idx));
                }
            }
            if (ptrAdr) {
                Describe pointee = describeValue( pointeeTy, ptrAdr, seen );
                res = ptr + " " + pointee.first;
            } else
                res = ptr + " (INVALID ADDRESS)";
            seen.insert( std::make_pair( idx, pointeeTy ) );
        }
    }
    return res;
}
#endif

Interpreter::Describe Interpreter::describeValue( Type *t, char *where, DescribeSeen &seen ) {
    std::string res;
    if ( t->isIntegerTy() ) {
        if ( t->isIntegerTy( 32 ) )
            res = wibble::str::fmt( *(int32_t*) where);
        if ( t->isIntegerTy( 8 ) )
            res = wibble::str::fmt( int( *(int8_t *) where ) );
        where += t->getPrimitiveSizeInBits() / 8;
    } else if ( t->isPointerTy() ) {
        assert_die();
        /* res = describePointer( t, *(intptr_t*) where, seen );
           where += t->getPrimitiveSizeInBits() / 8; */
    } else if ( t->getPrimitiveSizeInBits() ) {
        res = "?";
        where += t->getPrimitiveSizeInBits() / 8;
    } else if ( t->isAggregateType() ) {
        Describe sub = describeAggregate( t, where, seen );
        res = sub.first;
        where = sub.second;
    }
    return std::make_pair( res, where );
}

std::string Interpreter::describeValue( ProgramInfo::Value v, int thread, DescribeSeen *seen ) {
    std::string str, name;
    DescribeSeen _seen;
    if ( !seen )
        seen = &_seen;

    Value *val = info.llvmvaluemap[ v ];
    Type *type = val->getType();

    if ( val->getValueName() ) {
        name = val->getValueName()->getKey();
        if ( name.find( '.' ) != std::string::npos )
           return "";
        str = name + " = ";
        if ( type->isPointerTy() ) {
            assert_die(); // str += describePointer ...
        } else
            str += describeValue( type, state.dereference( v, thread ), *seen ).first;
    }
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
        } else
            vec.push_back( "<not started>" );

        auto function = info.function( state.frame( c ).pc );
        for ( auto v = function.values.begin(); v != function.values.end(); ++v )
        {
            std::string vdes = describeValue( *v, c, &seen );
            if ( !vdes.empty() )
                vec.push_back( vdes );
        }
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
