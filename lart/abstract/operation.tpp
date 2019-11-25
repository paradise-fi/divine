// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>

#pragma once

namespace lart::abstract
{
    template< Operation::Type type >
    template< typename Value, typename Builder >
    Operation Construct< type >::operation( Value * val, Builder & builder )
    {
        auto m = util::get_module( val );
        auto fn = util::get_or_insert_function( m, function_type( val ), name( val ) );
        auto call = builder.CreateCall( fn, arguments( val ) );
        return Operation( call, type, false /* placeholder */ );
    }

    template< Operation::Type T >
    Operation Construct< T >::operation( llvm::Instruction * inst )
    {
        llvm::IRBuilder<> irb{ inst->getContext() };

        if constexpr ( T == Type::PHI ) {
            irb.SetInsertPoint( inst->getParent()->getFirstNonPHI() );
        } else {
            irb.SetInsertPoint( inst );
        }

        Operation ph = operation( inst, irb );

        if constexpr ( T != Type::PHI ) {
            ph.inst->moveAfter( inst );
        }

        return ph;
    }

    template< Operation::Type T >
    llvm::Type * Construct< T >::output( llvm::Value * val )
    {
        auto m = util::get_module( val );

        if constexpr ( T == Type::ToBool ) {
            return llvm::Type::getInt1Ty( m->getContext() );
        }

        if ( auto inst = llvm::dyn_cast< llvm::Instruction >( val ); inst )
            if ( !Operation::is( inst ) && is_faultable( inst ) )
                return val->getType();

        // TODO what about operations returning concrete value?
        return llvm::Type::getInt8PtrTy( m->getContext() );
    }

    template< Operation::Type T >
    auto Construct< T >::arguments( llvm::Value * val )
        -> std::vector< llvm::Value * >
    {
        if constexpr ( T == Type::Store || T == Type::Freeze ) {
            auto s = llvm::cast< llvm::StoreInst >( val );
            return { s->getValueOperand(), s->getPointerOperand() };
        }

        if constexpr ( T == Type::Load || T == Type::Thaw ) {
            auto l = llvm::cast< llvm::LoadInst >( val );
            return { l->getPointerOperand() };
        }

        /*if constexpr ( is_mem_intrinsic( T ) ) {
            auto mem = llvm::cast< llvm::MemIntrinsic >( val );
            auto dst = mem->getArgOperand( 0 );
            auto val = mem->getArgOperand( 1 );
            auto len = mem->getArgOperand( 2 );
            return { dst, val, len };
        }*/

        return { val };
    }

    template< Operation::Type T >
    llvm::FunctionType * Construct< T >::function_type( llvm::Value * val )
    {
        return llvm::FunctionType::get( output( val ), types_of( arguments( val ) ), false );
    }

    template< Operation::Type T >
    std::string Construct< T >::name_suffix( llvm::Value * val )
    {
        std::string suffix = Operation::TypeTable[ T ];

        suffix += "." + llvm_name( output( val ) );

        if constexpr ( T == Type::Store || T == Type::Freeze ) {
            auto s = llvm::cast< llvm::StoreInst >( val );
            return suffix + "." + llvm_name( s->getValueOperand()->getType() );
        // } else if constexpr ( is_mem_intrinsic( T ) ) {
        //    auto intr = llvm::cast< llvm::MemIntrinsic >( val );
        //    auto dst = intr->getRawDest()->getType();
        //    return suffix + "." + llvm_name( dst->getPointerElementType() );
        //
        } else {
            if ( auto aggr = llvm::dyn_cast< llvm::StructType >( val->getType() );
                    aggr && aggr->hasName() ) {
                return suffix + "." + aggr->getName().str();
            } else {
                return suffix + "." + llvm_name( val->getType() );
            }
        }
    }

    template< Operation::Type T >
    std::string Construct< T >::name( llvm::Value * val )
    {
        if ( auto inst = llvm::dyn_cast< llvm::Instruction >( val ) )
            if ( Operation::is( inst ) )
                return name( inst->getOperand( 0 ) );
        return prefix + name_suffix( val );
    }

} // namespace lart::abstract
