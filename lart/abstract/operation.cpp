// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>

#include <lart/abstract/operation.h>

#include <lart/abstract/stash.h>
#include <lart/abstract/meta.h>

namespace lart::abstract
{
    const Bimap< Operation::Type, std::string > Operation::TypeTable = {
         { Operation::Type::PHI    , "phi"     }
        ,{ Operation::Type::GEP    , "gep"     }
        ,{ Operation::Type::Thaw   , "thaw"    }
        ,{ Operation::Type::Freeze , "freeze"  }
        ,{ Operation::Type::ToBool , "tobool"  }
        ,{ Operation::Type::Assume , "assume"  }
        ,{ Operation::Type::Store  , "store"   }
        ,{ Operation::Type::Load   , "load"    }
        ,{ Operation::Type::Cmp    , "cmp"     }
        ,{ Operation::Type::Cast   , "cast"    }
        ,{ Operation::Type::Binary , "binary"  }
        ,{ Operation::Type::BinaryFaultable, "binary.faultabel"  }
        ,{ Operation::Type::Lift   , "lift"    }
        ,{ Operation::Type::Lower  , "lower"   }
        ,{ Operation::Type::Call   , "call"    }
        ,{ Operation::Type::Memcpy , "memcpy"  }
        ,{ Operation::Type::Memmove, "memmove" }
        ,{ Operation::Type::Memset , "memset"  }
    };

    Operation OperationBuilder::construct( llvm::Instruction * inst )
    {
        if ( auto phi = llvm::dyn_cast< llvm::PHINode >( inst ) ) {
            return construct< Type::PHI >( phi );
        }

        if ( auto gep = llvm::dyn_cast< llvm::GetElementPtrInst >( inst ) ) {
            return construct< Type::GEP >( inst );
        }

        if ( auto store = llvm::dyn_cast< llvm::StoreInst >( inst ) ) {
            UNREACHABLE( "not implemented" );
        }

        if ( auto load = llvm::dyn_cast< llvm::LoadInst >( inst ) ) {
            if ( meta::has( load, meta::tag::operation::thaw ) )
                return construct< Type::Thaw >( load );
            else
                UNREACHABLE( "not implemented" );
        }

        if ( auto cmp = llvm::dyn_cast< llvm::CmpInst >( inst ) ) {
            return construct< Type::Cmp >( cmp );
        }

        if ( auto un = llvm::dyn_cast< llvm::CastInst >( inst ) ) {
            return construct< Type::Cast >( un );
        }

        if ( auto bin = llvm::dyn_cast< llvm::BinaryOperator >( inst ) ) {
            if ( is_faultable( inst ) )
                return construct< Type::BinaryFaultable >( bin );
            else
                return construct< Type::Binary >( bin );
        }

        if ( auto call = llvm::dyn_cast< llvm::CallInst >( inst ) ) {
            if ( llvm::isa< llvm::MemSetInst >( call ) )
                return construct< Type::Memset >( call );
            if ( llvm::isa< llvm::MemCpyInst >( call ) )
                return construct< Type::Memcpy >( call );
            if ( llvm::isa< llvm::MemMoveInst >( call ) )
                return construct< Type::Memmove >( call );
            return construct< Type::Call >( call );
        }

        UNREACHABLE( "Unsupported operation type" );
    }

    bool idempotent( llvm::Value * val )
    {
        return util::is_one_of< llvm::BitCastInst, llvm::IntToPtrInst, llvm::PtrToIntInst >( val );
    }

    void Matched::init( llvm::Module & m )
    {
        for ( auto call : abstract_calls( m ) ) {
            auto dual = call->getNextNonDebugInstruction();
            concrete[ dual ] = call;
            abstract[ call ] = dual;
            match_idempotent( call, dual );
        }

        for ( auto op : operations( m ) ) {
            auto concrete = op.inst->getPrevNode();
            match( op.type, op.inst, concrete );
        }

        for ( auto intr : unpacked_arguments( &m ) ) {
            auto call = llvm::cast< llvm::CallInst >( intr );

            auto c = call->getArgOperand( 0 );
            auto a = call->getArgOperand( 1 );

            abstract[ c ] = a;
            concrete[ a ] = c;

            match_idempotent( c, a );
        }

        const auto tag = meta::tag::operation::phi;
        for ( auto phi : meta::enumerate< llvm::PHINode >( m, tag ) ) {
            auto dual = phi->getNextNode();
            abstract[ phi ] = dual;
            concrete[ dual ] = phi;

            match_idempotent( phi, dual );
        }
    }

    void Matched::match( Operation::Type type, llvm::Value * a, llvm::Value * c )
    {
        if ( type == Operation::Type::Assume )
            return;

        if ( type == Operation::Type::ToBool ) {
            concrete[ a ] = concrete[ c ];
        } else {
            auto ai = llvm::cast< llvm::Instruction >( a );
            auto ci = llvm::cast< llvm::Instruction >( c );

            if ( is_faultable( ci ) ) {
                concrete[ a ] = c;
                auto unstash = ai->getNextNode();
                abstract[ c ] = unstash;
                concrete[ unstash ] = c;
                match_idempotent( c, unstash );
            } else {
                concrete[ a ] = c;
                abstract[ c ] = a;
                match_idempotent( c, a );
            }
        }
    }

    void Matched::match_idempotent( llvm::Value * v, llvm::Value * dual )
    {
        for ( auto u : v->users() ) {
            if ( idempotent( u ) )
                abstract[ u ] = dual;
            // TODO compute transitively
        }
    }

} // namespace lart::abstract
