// -*- C++ -*- (c) 2016-2019 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/InstVisitor.h>
DIVINE_UNRELAX_WARNINGS

#include <deque>
#include <memory>
#include <brick-types>
#include <lart/abstract/domain.h>

namespace lart::abstract {


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


    struct LatticeValue
    {

        struct Top { };
        struct Bottom { };

        using Value = std::variant< Bottom, Top >;

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

        Value value = Bottom{};
    };


    template< typename Core >
    struct Onion;

    template< typename Core >
    using Layer = std::shared_ptr< Onion< Core > >;

    template< typename Core >
    using OnionLayer = brick::types::Union< Core, Layer< Core > >;

    template< typename Core >
    struct Onion : OnionLayer< Core >
                 , std::enable_shared_from_this< Onion< Core > >
    {
        using typename OnionLayer< Core >::Union;

        Onion( Core core = Core() ) : Union( core ) {}
        Onion( Layer< Core > layer ) : Union( layer ) {}

        bool is_covered() const
        {
            return this->template is< Layer< Core > >();
        }

        bool is_core() const
        {
            return this->template is< Core >();
        }

        void to_bottom()
        {
            this->template get< Core >().to_bottom();
        }

        void to_top()
        {
            ASSERT( is_core() );
            this->template get< Core >().to_top();
        }

        bool join( const Onion< Core > &other )
        {
            // TODO peel to the same level
            return core().join( other.core() );
        }

        Onion& peel() const noexcept
        {
            ASSERT( is_covered() );
            return *this->template get< Layer< Core > >();
        }

        Onion cover() noexcept
        {
            return { this->shared_from_this() };
        }

        Core& core() noexcept
        {
            auto layer = this;
            while ( !layer->is_core() )
                layer = &layer->peel();
            return layer->template get< Core >();
        }

        const Core& core() const noexcept
        {
            auto layer = this;
            while ( !layer->is_core() )
                layer = &layer->peel();
            return layer->template get< Core >();
        }
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
