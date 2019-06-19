// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>

#include <lart/abstract/operation.h>

namespace lart::abstract
{
    const Bimap< Operation::Type, std::string > Operation::TypeTable = {
         { Operation::Type::PHI    , "phi"     }
        ,{ Operation::Type::GEP    , "gep"     }
        ,{ Operation::Type::Thaw   , "thaw"    }
        ,{ Operation::Type::Freeze , "freeze"  }
        ,{ Operation::Type::Stash  , "stash"   }
        ,{ Operation::Type::Unstash, "unstash" }
        ,{ Operation::Type::ToBool , "tobool"  }
        ,{ Operation::Type::Assume , "assume"  }
        ,{ Operation::Type::Store  , "store"   }
        ,{ Operation::Type::Load   , "load"    }
        ,{ Operation::Type::Cmp    , "cmp"     }
        ,{ Operation::Type::Cast   , "cast"    }
        ,{ Operation::Type::Binary , "binary"  }
        ,{ Operation::Type::Lift   , "lift"    }
        ,{ Operation::Type::Lower  , "lower"   }
        ,{ Operation::Type::Call   , "call"    }
        ,{ Operation::Type::Memcpy , "memcpy"  }
        ,{ Operation::Type::Memmove, "memmove" }
        ,{ Operation::Type::Memset , "memset"  }
        ,{ Operation::Type::Union , "union"  }
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
            /*if ( meta.scalar() ) {
                return construct< Type::Freeze >( store );
            }

            if ( meta.pointer() ) {

            }

            if ( meta.content() ) {
                if ( is_base_type_in_domain( m, store->getValueOperand(), dom ) )
                    return construct< Type::Freeze >( store );
                else
                    return construct< Type::Store >( store );
            }*/
        }

        if ( auto load = llvm::dyn_cast< llvm::LoadInst >( inst ) ) {
            /*if ( meta.scalar() ) {
                if ( is_base_type_in_domain( m, inst, dom ) ) {
                    return construct< Type::Thaw >( load );
                }
            }

            if ( meta.pointer() ) {

            }

            if ( meta.content() ) {
                if ( is_base_type_in_domain( m, load, dom ) )
                    return construct< Type::Thaw >( load );
                else
                    return construct< Type::Load >( load );
            }*/
        }

        if ( auto cmp = llvm::dyn_cast< llvm::CmpInst >( inst ) ) {
            return construct< Type::Cmp >( cmp );
        }

        if ( auto un = llvm::dyn_cast< llvm::CastInst >( inst ) ) {
            return construct< Type::Cast >( un );
        }

        if ( auto bin = llvm::dyn_cast< llvm::BinaryOperator >( inst ) ) {
            return construct< Type::Binary >( bin );
        }

        if ( auto call = llvm::dyn_cast< llvm::CallInst >( inst ) ) {
            // ASSERT( is_transformable( call ) );
            if ( llvm::isa< llvm::MemSetInst >( call ) )
                return construct< Type::Memset >( call );
            if ( llvm::isa< llvm::MemCpyInst >( call ) )
                return construct< Type::Memcpy >( call );
            if ( llvm::isa< llvm::MemMoveInst >( call ) )
                return construct< Type::Memmove >( call );
            return construct< Type::Call >( call );
        }

        if ( auto ret = llvm::dyn_cast< llvm::ReturnInst >( inst ) ) {
            return construct< Type::Stash >( ret );
        }

        UNREACHABLE( "Unsupported operation type" );
    }

} // namespace lart::abstract
