// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <string_view>

#include <lart/abstract/domain.h>
#include <lart/abstract/util.h>
#include <lart/support/util.h>

namespace lart {
namespace abstract {

    struct Placeholder
    {
        enum class Type {
            PHI,
            Thaw,
            Freeze,
            Stash,
            Unstash,
            ToBool,
            Assume,
            Store,
            Load,
            Cmp,
            Cast,
            Binary,
            Lift,
            Lower,
            Call
        };

        enum class Level {
            Abstract, // represents abstract operation place
            Taint     // represents taint place
        };

        explicit Placeholder( llvm::Instruction * inst, Level level, Type type )
            : inst( inst ), level( level ), type( type )
        {}

        llvm::Instruction * inst;
        Level level;
        Type type;
    };

    template< Placeholder::Level Level >
    struct PlaceholderBuilder
    {
        static constexpr const char * prefix = "lart.placeholder.";

        PlaceholderBuilder( llvm::LLVMContext & ctx ) :
            ctx( ctx )
        {}

        Placeholder construct( llvm::Instruction * inst ) const
        {
            using Type = Placeholder::Type;

            auto m = inst->getModule();
            auto dom = get_domain( inst );
            auto meta = domain_metadata( *m, dom );

            if ( auto phi = llvm::dyn_cast< llvm::PHINode >( inst ) ) {
                return construct< Type::PHI >( phi );
            }

            if ( auto store = llvm::dyn_cast< llvm::StoreInst >( inst ) ) {
                if ( meta.scalar() ) {
                    if ( get_domain( store->getValueOperand() ) == dom ) {
                        return construct< Type::Freeze >( store );
                    }
                }

                if ( meta.pointer() ) {

                }

                if ( meta.content() ) {

                }
            }

            if ( auto load = llvm::dyn_cast< llvm::LoadInst >( inst ) ) {
                if ( meta.scalar() ) {
                    if ( is_base_type_in_domain( m, inst, dom ) ) {
                        return construct< Type::Thaw >( load );
                    }
                }

                if ( meta.pointer() ) {

                }

                if ( meta.content() ) {

                }
            }

            if ( auto cmp = llvm::dyn_cast< llvm::CmpInst >( inst ) ) {
                assert( !meta.content() );
                return construct< Type::Cmp >( cmp );
            }

            if ( auto un = llvm::dyn_cast< llvm::CastInst >( inst ) ) {
                assert( !meta.content() );
                return construct< Type::Cast >( un );
            }

            if ( auto bin = llvm::dyn_cast< llvm::BinaryOperator >( inst ) ) {
                assert( !meta.content() );
                return construct< Type::Binary >( bin );
            }

            if ( auto call = llvm::dyn_cast< llvm::CallInst >( inst ) ) {
                // TODO assert transformable call
                return construct< Type::Call >( call );
            }

            UNREACHABLE( "Unsupported placeholder type" );
        }

        llvm::Type * output( llvm::Instruction * inst ) const
        {
            static_assert( Level == Placeholder::Level::Abstract );

            auto m = inst->getModule();
            auto dom = get_domain( inst );
            if ( is_base_type_in_domain( m, inst, dom ) ) { // TODO make domain + op specific
                return abstract_type( inst->getType(), dom );
            } else {
                return inst->getType();
            }
        }

        llvm::FunctionType * function_type( llvm::Instruction * inst ) const
        {
            if constexpr ( Level == Placeholder::Level::Abstract ) {
                return llvm::FunctionType::get( output( inst ), { inst->getType() }, false );
            } else {
                UNREACHABLE( "Unsupported abstraction level" );
            }
        }

        std::string name( llvm::Instruction * inst ) const
        {
            if constexpr ( Level == Placeholder::Level::Abstract ) {
                auto out = output( inst );
                if ( auto aggr = llvm::dyn_cast< llvm::StructType >( out ) )
                    return prefix + aggr->getName().str();
                else
                    return prefix + llvm_name( out );
            } else {
                UNREACHABLE( "Unsupported abstraction level" );
            }
        }

        template< Placeholder::Type Type >
        Placeholder construct( llvm::Instruction * inst ) const
        {
            llvm::IRBuilder<> irb{ ctx };
            if constexpr ( Type == Placeholder::Type::PHI ) {
                irb.SetInsertPoint( inst->getParent()->getFirstNonPHI() );
            } else {
                irb.SetInsertPoint( inst );
            }

            auto m = inst->getModule();

            auto fn = util::get_or_insert_function( m, function_type( inst ), name( inst ) );
            auto ph = irb.CreateCall( fn, { inst } );

            auto dom = get_domain( inst );
            assert( is_base_type_in_domain( m, inst, dom ) );

            if constexpr ( Type != Placeholder::Type::PHI ) {
                ph->moveAfter( inst );
            }

            add_abstract_metadata( ph, dom );
            make_duals( inst, ph );

            if ( !is_base_type_in_domain( m, inst, dom ) ) {
                inst->replaceAllUsesWith( ph );
                ph->setArgOperand( 0, inst );
            }

            return Placeholder{ ph, Level, Type };
        }

    private:
        llvm::LLVMContext & ctx;
    };

    using AbstractPlaceholderBuilder = PlaceholderBuilder< Placeholder::Level::Abstract >;
    using TaintPlaceholderBuilder = PlaceholderBuilder< Placeholder::Level::Taint >;

} // namespace abstract
} // namespace lart
