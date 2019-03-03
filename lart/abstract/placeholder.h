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

        static const Bimap< Type, std::string > TypeTable;

        enum class Level {
            Abstract, // represents abstract operation place
            Taint     // represents taint place
        };

        static const Bimap< Level, std::string > LevelTable;

        explicit Placeholder( llvm::Instruction * inst, Level level, Type type )
            : inst( inst ), level( level ), type( type )
        {
            auto & ctx = inst->getContext();
            auto tname = llvm::MDString::get( ctx, TypeTable[ type ] );
            inst->setMetadata( meta::tag::placeholder::type, llvm::MDNode::get( ctx, tname ) );

            auto lname = llvm::MDString::get( ctx, LevelTable[ level ] );
            inst->setMetadata( meta::tag::placeholder::type, llvm::MDNode::get( ctx, lname ) );
        }

        explicit Placeholder( llvm::Instruction * inst )
            : inst( inst )
        {
            level = LevelTable[ meta::get( inst, meta::tag::placeholder::level ).value() ];
            type = TypeTable[ meta::get( inst, meta::tag::placeholder::type ).value() ];
        }

        friend std::ostream& operator<<( std::ostream & os, const Placeholder & ph ) {
            auto fn = llvm::cast< llvm::CallInst >( ph.inst )->getCalledFunction();
            os << "[" << fn->getName().str()
               << ", " << LevelTable[ ph.level ]
               << ", " << TypeTable[ ph.type ] << "]";
            return os;
        }

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

        llvm::Type * output( llvm::Value * val ) const
        {
            static_assert( Level == Placeholder::Level::Abstract );

            auto m = util::get_module( val );
            auto dom = get_domain( val );
            if ( is_base_type_in_domain( m, val, dom ) ) { // TODO make domain + op specific
                return abstract_type( val->getType(), dom );
            } else {
                return val->getType();
            }
        }

        llvm::FunctionType * function_type( llvm::Value * val ) const
        {
            if constexpr ( Level == Placeholder::Level::Abstract ) {
                return llvm::FunctionType::get( output( val ), { val->getType() }, false );
            } else {
                UNREACHABLE( "Unsupported abstraction level" );
            }
        }

        std::string name( llvm::Value * val ) const
        {
            if constexpr ( Level == Placeholder::Level::Abstract ) {
                if ( auto aggr = llvm::dyn_cast< llvm::StructType >( val->getType() ) )
                    return prefix + aggr->getName().str();
                else
                    return prefix + llvm_name( val->getType() );
            } else {
                UNREACHABLE( "Unsupported abstraction level" );
            }
        }


        template< Placeholder::Type Type, typename Value, typename Builder >
        Placeholder construct( Value * val, Builder & builder ) const
        {
            auto m = util::get_module( val );
            auto fn = util::get_or_insert_function( m, function_type( val ), name( val ) );
            auto ph = builder.CreateCall( fn, { val } );
            add_abstract_metadata( ph, get_domain( val ) );
            return Placeholder{ ph, Level, Type };
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

            auto ph = construct< Type >( inst, irb );

            if constexpr ( Type != Placeholder::Type::PHI ) { // TODO remove
                ph.inst->moveAfter( inst );
            }

            meta::make_duals( inst, ph.inst );
            assert( is_base_type_in_domain( inst->getModule(), inst, get_domain( inst ) ) );

            return ph;
        }

        template< Placeholder::Type Type >
        Placeholder construct( llvm::Argument * arg )
        {
            static_assert( Type == Placeholder::Type::Stash || Type == Placeholder::Type::Unstash );
            auto & entry = arg->getParent()->getEntryBlock();
            llvm::IRBuilder<> irb{ &entry, entry.getFirstInsertionPt() };
            return construct< Type >( arg, irb );
        }

    private:
        llvm::LLVMContext & ctx;
    };

    using AbstractPlaceholderBuilder = PlaceholderBuilder< Placeholder::Level::Abstract >;
    using TaintPlaceholderBuilder = PlaceholderBuilder< Placeholder::Level::Taint >;

} // namespace abstract
} // namespace lart
