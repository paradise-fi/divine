// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Type.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/query.h>
#include <lart/support/util.h>
#include <lart/abstract/domain.h>

#include <experimental/type_traits>

namespace lart::abstract
{
    #define ERROR( msg ) \
        throw std::runtime_error( msg );

    template< typename T >
    using module_t = decltype( T::module );

    template< typename T >
    using context_t = decltype( T::context );

    template< typename T >
    constexpr bool has_member_module = std::experimental::is_detected_v< module_t, T >;

    template< typename T >
    constexpr bool has_member_context = std::experimental::is_detected_v< context_t, T >;


    template< typename T, template< typename > class Derived >
    struct crtp
    {
        T& self() { return static_cast< T& >( *this ); }
        const T& self() const { return static_cast< const T& >( *this ); }
    private:
        crtp() { }
        friend Derived< T >;
    };


    template< typename Self >
    struct LLVMUtilBase : crtp< Self, LLVMUtilBase >
    {
        static constexpr bool has_module = has_member_module< Self >;
        static constexpr bool has_context = has_member_context< Self >;

        template< typename T = Self,
            typename = std::enable_if_t< has_member_module< T > > >
        auto & module() const noexcept
        {
            return this->self().module;
        }

        template< typename T = Self,
            typename = std::enable_if_t< has_member_module< T > > >
        auto & module() noexcept
        {
            return this->self().module;
        }

        auto & ctx() const noexcept
        {
            static_assert( has_context || has_module );
            if constexpr ( has_context ) {
                return this->self().context;
            } else {
                return module()->getContext();
            }
        }

        auto & ctx() noexcept
        {
            static_assert( has_context || has_module );
            if constexpr ( has_context ) {
                return this->self().context;
            } else {
                return module()->getContext();
            }
        }
    };

    template< typename Self >
    struct LLVMUtil : LLVMUtilBase< Self >
    {
        using Name = llvm::StringRef;
        using Type = llvm::Type *;
        using Types = std::vector< Type >;

        using LLVMUtilBase< Self >::ctx;

        using LLVMUtilBase< Self >::module;

        llvm::Function * get_function( Name name, Type ret, const Types &args )
        {
            auto fty = llvm::FunctionType::get( ret, args, false );
            return llvm::cast< llvm::Function >( module()->getOrInsertFunction( name, fty ) );
        }

        llvm::Argument * argument( llvm::Function * fn, size_t i ) const noexcept
        {
            return std::next( fn->arg_begin(), i );
        }

        // type utilities
        llvm::ConstantInt * i1( bool value ) const noexcept
        {
            return GetConstantInt( value );
        }

        llvm::ConstantInt * i8( uint8_t value ) const noexcept
        {
            return GetConstantInt( value );
        }

        llvm::ConstantInt * i16( uint16_t value ) const noexcept
        {
            return GetConstantInt( value );
        }

        llvm::ConstantInt * i32( uint32_t value ) const noexcept
        {
            return GetConstantInt( value );
        }

        llvm::ConstantInt * i64( uint64_t value ) const noexcept
        {
            return GetConstantInt( value );
        }

        template< typename T >
        constexpr auto bitwidth() const noexcept
        {
            static_assert( std::is_integral_v< T > );
            return std::numeric_limits< T >::digits;
        }

        template< typename T >
        llvm::ConstantInt * GetConstantInt( T value ) const noexcept
        {
            auto i = llvm::ConstantInt::get( iNTy< T >(), value );
            return llvm::cast< llvm::ConstantInt >( i );
        }

        llvm::Type * voidTy() const noexcept
        {
            return llvm::Type::getVoidTy( ctx() );
        }

        llvm::Type * i1Ty() const noexcept
        {
            return llvm::Type::getInt1Ty( ctx() );
        }

        llvm::Type * i8Ty() const noexcept
        {
            return llvm::Type::getInt8Ty( ctx() );
        }

        llvm::Type * i16Ty() const noexcept
        {
            return llvm::Type::getInt16Ty( ctx() );
        }

        llvm::Type * i32Ty() const noexcept
        {
            return llvm::Type::getInt32Ty( ctx() );
        }

        llvm::Type * i64Ty() const noexcept
        {
            return llvm::Type::getInt64Ty( ctx() );
        }

        template< typename T >
        llvm::Type * iNTy() const noexcept
        {
            static_assert( std::is_integral_v< T > );
            auto & c = const_cast< llvm::LLVMContext & >( ctx() );
            return llvm::Type::getIntNTy( c, bitwidth< T >() );
        }

        llvm::PointerType * i8PTy() const noexcept
        {
            return llvm::Type::getInt8PtrTy( ctx() );
        }

        llvm::PointerType * i16PTy() const noexcept
        {
            return llvm::Type::getInt16PtrTy( ctx() );
        }

        llvm::PointerType * i32PTy() const noexcept
        {
            return llvm::Type::getInt32PtrTy( ctx() );
        }

        llvm::PointerType * i64PTy() const noexcept
        {
            return llvm::Type::getInt64PtrTy( ctx() );
        }

        llvm::ConstantPointerNull * null_ptr( llvm::PointerType * type ) const noexcept
        {
            return llvm::ConstantPointerNull::get( type );
        }

        llvm::Value * undef( llvm::Type * ty ) const noexcept
        {
            return llvm::UndefValue::get( ty );
        }
    };

    template< typename IP, typename Callable, typename ArgPack >
    auto create_call( IP point, Callable callable, ArgPack args ) noexcept
    {
        llvm::IRBuilder<> irb( point );
        return irb.CreateCall( callable, args );
    }

    template< typename IP, typename Callable  >
    auto create_call( IP point, Callable callable ) noexcept
    {
        return create_call( point, callable, llvm::None );
    }

    template< typename IP, typename Callable, typename ArgPack >
    auto create_call_after( IP point, Callable callable, ArgPack args ) noexcept
    {
        auto call = create_call( point, callable, args );
        call->moveAfter( point );
        return call;
    }

    template< typename IP, typename Callable >
    auto create_call_after( IP point, Callable callable ) noexcept
    {
        return create_call_after( point, callable, llvm::None );
    }

    static bool is_faultable( llvm::Instruction * inst ) noexcept
    {
        using Inst = llvm::Instruction;
        if ( auto bin = llvm::dyn_cast< llvm::BinaryOperator >( inst ) ) {
            auto op = bin->getOpcode();
            return op == Inst::UDiv ||
                   op == Inst::SDiv ||
                   op == Inst::FDiv ||
                   op == Inst::URem ||
                   op == Inst::SRem ||
                   op == Inst::FRem;
        }

        // TODO if call is in table
        return llvm::isa< llvm::CallInst >( inst );
    }

    static inline llvm::Value * lower_constant_expr_call( llvm::Value * val ) {
        auto ce = llvm::dyn_cast< llvm::ConstantExpr >( val );
        if ( !ce )
            return val;
        if ( ce->getNumUses() == 0 )
            return nullptr;
        if ( auto orig = llvm::dyn_cast< llvm::CallInst >( *ce->user_begin() ) ) {
            auto fn = ce->getOperand( 0 );
            llvm::IRBuilder<> irb( orig );
            llvm::Value * call = irb.CreateCall( fn );
            if ( call->getType() != orig->getType() )
                call = irb.CreateBitCast( call, orig->getType() );
            orig->replaceAllUsesWith( call );
            orig->eraseFromParent();
            return call;
        }

        return nullptr;
    }

    template< const char * tag >
    auto calls_with_tag( llvm::Module & m )
    {
        return query::query( m ).flatten().flatten()
            .map( query::llvmdyncast< llvm::CallInst > )
            .filter( query::notnull )
            .filter( [&] ( auto * call ) {
                return call->getMetadata( tag );
            } )
            .freeze();
    }

    static inline auto abstract_calls( llvm::Module & m )
    {
        return calls_with_tag< meta::tag::abstract >( m );
    }

    using Types = std::vector< llvm::Type * >;
    using Values = std::vector< llvm::Value * >;
    using Functions = std::vector< llvm::Function * >;

    Functions resolve_function( llvm::Module *m, llvm::Value *fn );

    static inline Functions resolve_call( const llvm::CallSite &call )
    {
        return resolve_function( call.getInstruction()->getModule(), call.getCalledValue() );
    }

    template< typename Y >
    void resolve_function( llvm::Module *m, llvm::Value *fn, Y yield )
    {
        for ( auto f : resolve_function( m, fn ) )
            yield( f );
    }

    template< typename Values >
    Types types_of( const Values & vs )
    {
        return query::query( vs ).map( [] ( const auto & v ) {
            return v->getType();
        } ).freeze();
    }

    std::string llvm_name( llvm::Type * type );

    namespace {
        using Predicate = llvm::CmpInst::Predicate;
    }

    static const std::unordered_map< llvm::CmpInst::Predicate, std::string > PredicateTable = {
        { Predicate::FCMP_FALSE, "ffalse" },
        { Predicate::FCMP_OEQ, "foeq" },
        { Predicate::FCMP_OGT, "fogt" },
        { Predicate::FCMP_OGE, "foge" },
        { Predicate::FCMP_OLT, "folt" },
        { Predicate::FCMP_OLE, "fole" },
        { Predicate::FCMP_ONE, "fone" },
        { Predicate::FCMP_ORD, "ford" },
        { Predicate::FCMP_UNO, "funo" },
        { Predicate::FCMP_UEQ, "fueq" },
        { Predicate::FCMP_UGT, "fugt" },
        { Predicate::FCMP_UGE, "fuge" },
        { Predicate::FCMP_ULT, "fult" },
        { Predicate::FCMP_ULE, "fule" },
        { Predicate::FCMP_UNE, "fune" },
        { Predicate::FCMP_TRUE, "ftrue" },
        { Predicate::ICMP_EQ, "eq" },
        { Predicate::ICMP_NE, "ne" },
        { Predicate::ICMP_UGT, "ugt" },
        { Predicate::ICMP_UGE, "uge" },
        { Predicate::ICMP_ULT, "ult" },
        { Predicate::ICMP_ULE, "ule" },
        { Predicate::ICMP_SGT, "sgt" },
        { Predicate::ICMP_SGE, "sge" },
        { Predicate::ICMP_SLT, "slt" },
        { Predicate::ICMP_SLE, "sle" }
    };
} // namespace lart::abstract
