// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>

#pragma once

namespace lart::abstract
{
    constexpr bool to_concrete( Placeholder::Type T )
    {
        return T == Placeholder::Type::ToBool || T == Placeholder::Type::Lower;
    }

    template< Placeholder::Type _T >
    constexpr bool is( Placeholder::Type T ) { return T == _T; }

    template< Placeholder::Level L, Placeholder::Type T >
    template< typename Value, typename Builder >
    Placeholder Construct< L, T >::placeholder( Value * val, Builder & builder )
    {
        auto m = util::get_module( val );
        auto fn = util::get_or_insert_function( m, function_type( val ), name( val ) );
        meta::set( fn, meta::tag::placeholder::function );
        auto ph = builder.CreateCall( fn, arguments( val ) );
        meta::abstract::inherit( ph, val );
        return Placeholder{ ph, L, T };
    }

    template< Placeholder::Level L, Placeholder::Type T >
    Placeholder Construct< L, T >::placeholder( llvm::Instruction * inst )
    {
        static_assert( abstract_level( L ) );
        assert( !llvm::isa< llvm::ReturnInst >( inst ) );
        llvm::IRBuilder<> irb{ inst->getContext() };
        if constexpr ( T == Type::PHI ) {
            irb.SetInsertPoint( inst->getParent()->getFirstNonPHI() );
        } else {
            irb.SetInsertPoint( inst );
        }

        Placeholder ph = placeholder( inst, irb );

        if constexpr ( T != Type::PHI ) {
            if ( !llvm::isa< llvm::ReturnInst >( inst ) ) {
                ph.inst->moveAfter( inst );
            }
        }

        if constexpr ( T != Type::ToBool ) {
            meta::make_duals( inst, ph.inst );
        }

        return ph;
    }

    template< Placeholder::Level L, Placeholder::Type T >
    Placeholder Construct< L, T >::placeholder( llvm::Argument * arg )
    {
        static_assert( abstract_level( L ) );
        static_assert( is< Type::Stash >( T ) || is< Type::Unstash >( T ) );
        auto & entry = arg->getParent()->getEntryBlock();
        llvm::IRBuilder<> irb{ &entry, entry.getFirstInsertionPt() };
        return placeholder( arg, irb );
    }

    template< Placeholder::Level L, Placeholder::Type T >
    llvm::Type * Construct< L, T >::output( llvm::Value * val )
    {
        auto m = util::get_module( val );
        auto dom = Domain::get( val );

        if constexpr ( T == Type::ToBool ) {
            return llvm::Type::getInt1Ty( m->getContext() );
        } else {
            if ( is_base_type_in_domain( m, val, dom ) ) { // TODO make domain + op specific
                if constexpr ( abstract_level( L ) ) {
                    return abstract_type( val->getType(), dom );
                }
                if constexpr ( concrete_level( L ) ) {
                    return DomainMetadata::get( m, dom ).base_type();
                }
            } else {
                return val->getType();
            }

            UNREACHABLE( "Unsupported abstraction level" );
        }
    }

    template< Placeholder::Level L, Placeholder::Type T >
    auto Construct< L, T >::arguments( llvm::Value * val )
        -> std::vector< llvm::Value * >
    {
        if constexpr ( is< Type::Store >( T ) || is< Type::Freeze >( T ) ) {
            auto s = llvm::cast< llvm::StoreInst >( val );
            return { s->getValueOperand(), s->getPointerOperand() };
        }

        if constexpr ( is< Type::Load >( T ) || is< Type::Thaw >( T ) ) {
            auto l = llvm::cast< llvm::LoadInst >( val );
            return { l->getPointerOperand() };
        }

        return { val };
    }

    template< Placeholder::Level L, Placeholder::Type T >
    llvm::FunctionType * Construct< L, T >::function_type( llvm::Value * val )
    {
        return llvm::FunctionType::get( output( val ), types_of( arguments( val ) ), false );
    }


    template< Placeholder::Level L, Placeholder::Type T >
    std::string Construct< L, T >::name_suffix( llvm::Value * val )
    {
        std::string suffix = Placeholder::TypeTable[ T ];

        if constexpr ( is< Type::Store >( T ) || is< Type::Freeze >( T ) ) {
            auto s = llvm::cast< llvm::StoreInst >( val );
            return suffix + "." + llvm_name( s->getValueOperand()->getType() );
        } else {
            if ( auto aggr = llvm::dyn_cast< llvm::StructType >( val->getType() ) ) {
                return suffix + "." + aggr->getName().str();
            } else {
                return suffix + "." + llvm_name( val->getType() );
            }
        }
    }

    template< Placeholder::Level L, Placeholder::Type T >
    std::string Construct< L, T >::name( llvm::Value * val )
    {
        if ( auto inst = llvm::dyn_cast< llvm::Instruction >( val ) )
            if ( Placeholder::is( inst ) )
                return name( inst->getOperand( 0 ) );

        auto suffix = name_suffix( val );

        if constexpr ( abstract_level( L ) ) {
            std::string infix = ".abstract.";
            return prefix + infix + suffix;
        }

        if constexpr ( concrete_level( L ) ) {
            auto dom = Domain::get( val );
            std::string infix = "." + dom.name() + ".";
            return prefix + infix + suffix;
        }

        UNREACHABLE( "Unsupported abstraction level" );
    }

    template< Placeholder::Type T, typename Map >
    auto operand( llvm::Instruction * inst, const Map & map ) -> llvm::Value *
    {
        if constexpr ( T == Placeholder::Type::Stash )
            return inst->getOperand( 0 );

        if ( meta::has_dual( inst ) )
            return meta::get_dual( inst );

        auto arg = llvm::cast< llvm::Instruction >( inst->getOperand( 0 ) );
        ASSERT( Placeholder::is( arg ) );
        return map.at( Placeholder{ arg } ).inst;
    }

    template< Placeholder::Level L, Placeholder::Type T >
    Placeholder Concretize< L, T >::placeholder( const Placeholder & ph, Concretize< L, T >::Map & map )
    {
        static_assert( concrete_level( L ) );
        assert( abstract_level( ph.level ) );

        llvm::IRBuilder<> irb{ ph.inst };
        auto op = operand< T >( ph.inst, map );

        using Builder = Construct< L, T >;
        auto conc = Builder().placeholder( op, irb );

        if constexpr ( !is_one_of< Type::Assume, Type::ToBool, Type::Stash >( T ) ) {
            meta::make_duals( op, conc.inst );
        }

        if constexpr ( to_concrete( T ) ) {
            for ( auto & u : ph.inst->uses() ) {
                if ( auto inst = llvm::dyn_cast< llvm::Instruction >( u.getUser() ) )
                    if ( !Placeholder::is( inst ) )
                        u.set( conc.inst );
            }
        }

        map.emplace( ph, conc );
        return conc;
    }


    template< Placeholder::Level L >
    Placeholder PlaceholderBuilder< L >::construct( llvm::Instruction * inst )
    {
        static_assert( abstract_level( L ) );

        auto m = inst->getModule();
        auto dom = Domain::get( inst );
        auto meta = DomainMetadata::get( m, dom );

        if ( auto phi = llvm::dyn_cast< llvm::PHINode >( inst ) ) {
            return construct< Type::PHI >( phi );
        }

        if ( auto gep = llvm::dyn_cast< llvm::GetElementPtrInst >( inst ) ) {
            ASSERT( meta.content() );
            return construct< Type::GEP >( inst );
        }

        if ( auto store = llvm::dyn_cast< llvm::StoreInst >( inst ) ) {
            if ( meta.scalar() ) {
                return construct< Type::Freeze >( store );
            }

            if ( meta.pointer() ) {

            }

            if ( meta.content() ) {
                if ( is_base_type_in_domain( m, store->getValueOperand(), dom ) )
                    return construct< Type::Freeze >( store );
                else
                    return construct< Type::Store >( store );
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
                if ( is_base_type_in_domain( m, load, dom ) )
                    return construct< Type::Thaw >( load );
                else
                    return construct< Type::Load >( load );
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
            ASSERT( is_transformable( call ) );
            return construct< Type::Call >( call );
        }

        if ( auto ret = llvm::dyn_cast< llvm::ReturnInst >( inst ) ) {
            return construct< Type::Stash >( ret );
        }

        UNREACHABLE( "Unsupported placeholder type" );
    }

    template< Placeholder::Level L >
    Placeholder PlaceholderBuilder< L >::concretize( const Placeholder & ph )
    {
        static_assert( concrete_level( L ) );

        switch ( ph.type )
        {
            case Type::PHI:
                return concretize< Type::PHI >( ph );
            case Type::GEP:
                return concretize< Type::GEP >( ph );
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
        }

        UNREACHABLE( "Unsupported placeholder type" );
    }
} // namespace lart::abstract
