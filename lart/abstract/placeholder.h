// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/domain.h>
#include <lart/abstract/util.h>
#include <lart/support/util.h>

namespace lart::abstract {

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
            meta::set( inst, meta::tag::placeholder::level, LevelTable[ level ] );
            meta::set( inst, meta::tag::placeholder::type, TypeTable[ type ] );
        }

        explicit Placeholder( llvm::Instruction * inst )
            : inst( inst )
        {
            level = LevelTable[ meta::get( inst, meta::tag::placeholder::level ).value() ];
            type = TypeTable[ meta::get( inst, meta::tag::placeholder::type ).value() ];
        }

        static bool is( llvm::Instruction * inst )
        {
            return meta::has( inst, meta::tag::placeholder::type );
        }

        static std::vector< Placeholder > enumerate( llvm::Module & m )
        {
            return query::query( m ).flatten().flatten()
                .map( query::refToPtr )
                .filter( Placeholder::is )
                .map( [] ( auto * inst ) { return Placeholder( inst ); } )
                .freeze();
        }

        friend std::ostream& operator<<( std::ostream & os, const Placeholder & ph )
        {
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
            auto dom = Domain::get( inst );
            auto meta = DomainMetadata::get( m, dom );

            if ( auto phi = llvm::dyn_cast< llvm::PHINode >( inst ) ) {
                return construct< Type::PHI >( phi );
            }

            if ( auto store = llvm::dyn_cast< llvm::StoreInst >( inst ) ) {
                if ( meta.scalar() ) {
                    if ( Domain::get( store->getValueOperand() ) == dom ) {
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

            if ( auto ret = llvm::dyn_cast< llvm::ReturnInst >( inst ) ) {
                return construct< Type::Stash >( ret );
            }

            UNREACHABLE( "Unsupported placeholder type" );
        }

        llvm::Type * output( llvm::Value * val ) const
        {
            static_assert( Level == Placeholder::Level::Abstract );

            auto m = util::get_module( val );
            auto dom = Domain::get( val );
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

            if constexpr ( Type != Placeholder::Type::Stash ) { // TODO  maybe not needed
                meta::abstract::inherit( ph, val );
            }
            return Placeholder{ ph, Level, Type };
        }

        template< Placeholder::Type Type >
        Placeholder construct( llvm::Instruction * inst ) const
        {
            assert( !llvm::isa< llvm::ReturnInst >( inst ) );

            llvm::IRBuilder<> irb{ ctx };
            if constexpr ( Type == Placeholder::Type::PHI ) {
                irb.SetInsertPoint( inst->getParent()->getFirstNonPHI() );
            } else {
                irb.SetInsertPoint( inst );
            }

            Placeholder ph = construct< Type >( inst, irb );

            ph.inst->moveAfter( inst );

            meta::make_duals( inst, ph.inst );
            assert( is_base_type_in_domain( inst->getModule(), inst, Domain::get( inst ) ) );

            return ph;
        }

        template< Placeholder::Type Type >
        Placeholder construct( llvm::ReturnInst * ret )
        {
            static_assert( Type == Placeholder::Type::Stash );
            llvm::IRBuilder<> irb{ ret };
            Placeholder ph = construct< Type >( stash_value( ret ), irb );

            meta::make_duals( ret, ph.inst );
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
        llvm::Value * stash_value( llvm::ReturnInst * ret ) const noexcept
        {
            auto val = ret->getReturnValue();
            if ( has_placeholder( val ) )
                return get_placeholder( val );

            if ( llvm::isa< llvm::Argument >( val ) || llvm::isa< llvm::CallInst >( val ) ) {
                assert( has_placeholder( val, "lart.unstash.placeholder" ) );
                return get_unstash_placeholder( val );
            }

            UNREACHABLE( "Stashing non abstract return value" );
            //auto aty = abstract_type( val->getType(), Domain::get( call ) );
            //return UndefValue::get( aty );
        }

        llvm::LLVMContext & ctx;
    };

    static inline auto placeholders( llvm::Module & m ) noexcept
    {
        return Placeholder::enumerate( m );
    }

    template< typename Filter >
    auto placeholders( llvm::Module & m, Filter filter ) noexcept
    {
        return query::query( placeholders( m ) ).filter( filter ).freeze();
    }

    template< Placeholder::Type type >
    auto placeholders( llvm::Module & m ) noexcept
    {
        return placeholders( m, [] ( const auto & ph ) { return ph.type == type; } );
    }

    using AbstractPlaceholderBuilder = PlaceholderBuilder< Placeholder::Level::Abstract >;
    using TaintPlaceholderBuilder = PlaceholderBuilder< Placeholder::Level::Taint >;

} // namespace lart::abstract
