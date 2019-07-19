// -*- C++ -*- (c) 2016-2019 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/InstVisitor.h>
DIVINE_UNRELAX_WARNINGS

#include <deque>
#include <memory>
#include <lart/abstract/domain.h>

namespace lart::abstract {


template< typename Onion >
auto cover( const Onion &onion ) noexcept -> Onion
{
    return { std::make_shared< Onion >( onion ) };
}


template< typename Onion >
Onion peel( const Onion &onion ) noexcept
{
    using Ptr = typename Onion::OnionPtr;

    ASSERT( onion.is_covered() );
    return *std::get< Ptr >( onion._layer );
}


template< typename Onion >
auto& core( const Onion &onion ) noexcept
{
    using Core = typename Onion::Core;

    auto layer = onion;
    while ( !layer.is_core() )
        layer = peel( layer );
    return std::get< Core >( layer._layer );
}


struct DataFlowAnalysis
{
    using Task = std::function< void() >;

    void run( llvm::Module & );

    void propagate( llvm::Value *inst ) noexcept;

    void propagate_in( llvm::CallInst * call ) noexcept;
    void propagate_out( llvm::ReturnInst * ret ) noexcept;

    bool join( llvm::Value *, llvm::Value * ) noexcept;

    inline void push( Task && t ) noexcept {
        _tasks.emplace_back( std::move( t ) );
    }


    template< typename CoreT >
    struct Onion
    {
        using Core = CoreT;

        using OnionPtr = std::shared_ptr< Onion< Core > >;
        using Layer = std::variant< OnionPtr, Core >;

        bool is_covered() const
        {
            return std::holds_alternative< OnionPtr >( _layer );
        }

        bool is_core() const
        {
            return std::holds_alternative< Core >( _layer );
        }

        void to_bottom()
        {
            ASSERT( is_core() );
            std::get< Core >( _layer ).to_bottom();
        }

        void to_top()
        {
            ASSERT( is_core() );
            std::get< Core >( _layer ).to_top();
        }

        bool join( const Onion< Core > &other )
        {
            // TODO peel to the same level
            return core( *this ).join( core( other ) );
        }

        Layer _layer = Core{};
    };


    struct LatticeValue
    {

        struct Top { };
        struct Bottom { };

        using Value = std::variant< Top, Bottom >;

        void to_bottom() noexcept { value = { Bottom{} }; }
        void to_top() noexcept { value = { Top{} }; }

        static inline bool is_bottom( const Value &v ) noexcept
        {
            return std::holds_alternative< Bottom >( v );
        }

        static inline bool is_top( const Value &v ) noexcept
        {
            return std::holds_alternative< Top >( v );
        }

        bool join( const LatticeValue &other ) noexcept
        {
            if ( is_top( value ) )
                return false;
            if ( is_top( other.value ) ) {
                to_top();
                return true; // change of value
            }
            return false;
        }

        Value value;
    };

    template< typename I >
    struct IntervalMap
    {
        bool has( llvm::Value * val ) const noexcept
        {
            return _intervals.count( val );
        }

        I& operator[]( llvm::Value * val ) noexcept
        {
            if ( !has( val ) ) {
                _intervals[ val ].to_bottom();
            }
            return _intervals[ val ];
        }

        decltype(auto) begin() { return _intervals.begin(); }
        decltype(auto) begin() const { return _intervals.begin(); }

        decltype(auto) end() { return _intervals.end(); }
        decltype(auto) end() const { return _intervals.end(); }

        std::map< llvm::Value *, I > _intervals;
    };

    inline auto& interval( llvm::Value * val ) noexcept
    {
        return _intervals[ val ];
    }

    using MapValue = Onion< LatticeValue >;

    void add_meta( llvm::Value *value, const MapValue& mval ) noexcept;

    IntervalMap< MapValue > _intervals;

    std::deque< Task > _tasks;
};

} // namespace lart::abstract
