// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/domain.h>
#include <lart/abstract/util.h>
#include <lart/support/util.h>

#include <brick-types>

namespace lart::abstract {

    struct Placeholder : brick::types::Ord
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
            Concrete  // represents concretized abstract place
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

        auto as_tuple() const noexcept
        {
            return std::tuple( inst, level, type );
        }

        bool operator==( const Placeholder & other ) const
        {
            return this->as_tuple() == other.as_tuple();
        }

        bool operator<( const Placeholder & other ) const
        {
            return this->as_tuple() < other.as_tuple();
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

    struct Empty {};

    struct ConcretizationBuilder
    {
        std::map< Placeholder, Placeholder > concretized;
    };

    template< Placeholder::Level Level >
    struct PlaceholderBuilder
        : std::conditional_t< Level == Placeholder::Level::Concrete, ConcretizationBuilder, Empty >
    {
        using Type = Placeholder::Type;

        static constexpr const char * prefix = "lart.placeholder";

        PlaceholderBuilder( llvm::LLVMContext & ctx ) :
            ctx( ctx )
        {}

        Placeholder construct( llvm::Instruction * inst ) const
        {
            static_assert( Level == Placeholder::Level::Abstract );

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

        Placeholder concretize( const Placeholder & ph )
        {
            static_assert( Level == Placeholder::Level::Concrete );
            assert( ph.level == Placeholder::Level::Abstract );

            switch ( ph.type )
            {
                case Type::PHI:
                    return concretize< Type::PHI >( ph );
                case Type::Thaw:
                    return concretize< Type::Thaw >( ph );
                case Type::Freeze:
                    return concretize< Type::Freeze >( ph );
                case Type::Stash:
                    return concretize< Type::Stash >( ph );
                case Type::Unstash:
                    return concretize< Type::Unstash >( ph );
                case Type::ToBool:
                    return concretize< Type::ToBool >( ph );
                case Type::Assume:
                    return concretize< Type::Assume >( ph );
                case Type::Store:
                    return concretize< Type::Store >( ph );
                case Type::Load:
                    return concretize< Type::Load >( ph );
                case Type::Cmp:
                    return concretize< Type::Cmp >( ph );
                case Type::Cast:
                    return concretize< Type::Cast >( ph );
                case Type::Binary:
                    return concretize< Type::Binary >( ph );
                case Type::Lift:
                    return concretize< Type::Lift >( ph );
                case Type::Lower:
                    return concretize< Type::Lower >( ph );
                case Type::Call:
                    return concretize< Type::Call >( ph );
                default:
                    UNREACHABLE( "Unsupported placeholder type" );
            }
        }

        template< Placeholder::Type T >
        Placeholder concretize( const Placeholder & ph )
        {
            static_assert( Level == Placeholder::Level::Concrete );
            assert( ph.level == Placeholder::Level::Abstract );

            llvm::IRBuilder<> irb{ ph.inst };
            auto op = ph.inst->getOperand( 0 );


            if ( auto inst = llvm::dyn_cast< llvm::Instruction >( op ) )
                if ( Placeholder::is( inst ) )
                    op = this->concretized.at( Placeholder{ inst } ).inst;

            auto conc = construct< T >( op, irb );

            if constexpr ( T != Type::Assume && T != Type::ToBool ) {
                if ( auto inst = llvm::dyn_cast< llvm::Instruction >( op ) ) {
                        meta::make_duals( inst, conc.inst );
                }
            }

            if constexpr ( T == Type::ToBool || T == Type::Lower ) {
                for ( auto & u : ph.inst->uses() ) {
                    if ( auto inst = llvm::dyn_cast< llvm::Instruction >( u.getUser() ) )
                        if ( !Placeholder::is( inst ) )
                            u.set( conc.inst );
                }
            }

            this->concretized.emplace( ph, conc );

            return conc;
        }

        template< Placeholder::Type T >
        llvm::Type * output( llvm::Value * val ) const
        {
            auto m = util::get_module( val );
            auto dom = Domain::get( val );

            if constexpr ( T == Type::ToBool ) {
                return llvm::Type::getInt1Ty( ctx );
            } else {
                if ( is_base_type_in_domain( m, val, dom ) ) { // TODO make domain + op specific
                    if constexpr ( Level == Placeholder::Level::Abstract ) {
                        return abstract_type( val->getType(), dom );
                    }
                    if constexpr ( Level == Placeholder::Level::Concrete ) {
                        return DomainMetadata::get( m, dom ).base_type();
                    }
                } else {
                    return val->getType();
                }

                UNREACHABLE( "Unsupported abstraction level" );
            }

        }

        template< Placeholder::Type T >
        llvm::FunctionType * function_type( llvm::Value * val ) const
        {
            return llvm::FunctionType::get( output< T >( val ), { val->getType() }, false );
        }

        template< Placeholder::Type T >
        std::string name( llvm::Value * val ) const
        {
            if ( auto inst = llvm::dyn_cast< llvm::Instruction >( val ) )
                if ( Placeholder::is( inst ) )
                    return name< T >( inst->getOperand( 0 ) );

            std::string suffix = Placeholder::TypeTable[ T ];

            if ( auto aggr = llvm::dyn_cast< llvm::StructType >( val->getType() ) ) {
                suffix += "." + aggr->getName().str();
            } else {
                suffix += "." + llvm_name( val->getType() );
            }

            if constexpr ( Level == Placeholder::Level::Abstract ) {
                std::string infix = ".abstract.";
                return prefix + infix + suffix;
            }

            if constexpr ( Level == Placeholder::Level::Concrete ) {
                auto dom = Domain::get( val );
                std::string infix = "." + dom.name() + ".";
                return prefix + infix + suffix;
            }

            UNREACHABLE( "Unsupported abstraction level" );
        }


        template< Placeholder::Type T, typename Value, typename Builder >
        Placeholder construct( Value * val, Builder & builder ) const
        {
            auto m = util::get_module( val );
            auto fn = util::get_or_insert_function( m, function_type< T >( val ), name< T >( val ) );
            auto ph = builder.CreateCall( fn, { val } );

            if constexpr ( T != Type::Stash ) { // TODO  maybe not needed
                meta::abstract::inherit( ph, val );
            }
            return Placeholder{ ph, Level, T };
        }

        template< Placeholder::Type T >
        Placeholder construct( llvm::Instruction * inst ) const
        {
            static_assert( Level == Placeholder::Level::Abstract );
            assert( !llvm::isa< llvm::ReturnInst >( inst ) );

            llvm::IRBuilder<> irb{ ctx };
            if constexpr ( T == Type::PHI ) {
                irb.SetInsertPoint( inst->getParent()->getFirstNonPHI() );
            } else {
                irb.SetInsertPoint( inst );
            }

            Placeholder ph = construct< T >( inst, irb );

            ph.inst->moveAfter( inst );

            if constexpr ( T != Type::ToBool ) {
                meta::make_duals( inst, ph.inst );
                assert( is_base_type_in_domain( inst->getModule(), inst, Domain::get( inst ) ) );
            }

            return ph;
        }

        template< Placeholder::Type T >
        Placeholder construct( llvm::ReturnInst * ret )
        {
            static_assert( Level == Placeholder::Level::Abstract );
            static_assert( T == Type::Stash );
            llvm::IRBuilder<> irb{ ret };
            Placeholder ph = construct< T >( stash_value( ret ), irb );

            meta::make_duals( ret, ph.inst );
            return ph;
        }

        template< Placeholder::Type T >
        Placeholder construct( llvm::Argument * arg )
        {
            static_assert( Level == Placeholder::Level::Abstract );
            static_assert( T == Type::Stash || T == Type::Unstash );
            auto & entry = arg->getParent()->getEntryBlock();
            llvm::IRBuilder<> irb{ &entry, entry.getFirstInsertionPt() };
            return construct< T >( arg, irb );
        }

    private:
        llvm::Value * stash_value( llvm::ReturnInst * ret ) const noexcept
        {
            static_assert( Level == Placeholder::Level::Abstract );
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

    template< Placeholder::Type T >
    auto placeholders( llvm::Module & m ) noexcept
    {
        return placeholders( m, [] ( const auto & ph ) { return ph.type == T; } );
    }

    template< Placeholder::Level L >
    auto placeholders( llvm::Module & m ) noexcept
    {
        return placeholders( m, [] ( const auto & ph ) { return ph.level == L; } );
    }

    using APlaceholderBuilder = PlaceholderBuilder< Placeholder::Level::Abstract >;
    using CPlaceholderBuilder = PlaceholderBuilder< Placeholder::Level::Concrete >;

} // namespace lart::abstract
