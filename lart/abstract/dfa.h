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

    void propagate( llvm::Value * inst ) noexcept;

    void propagate_in( llvm::CallInst * call ) noexcept;
    void propagate_out( llvm::ReturnInst * ret ) noexcept;

    bool propagate_wrap( llvm::Value * lhs, llvm::Value * rhs ) noexcept;
    bool propagate_identity( llvm::Value * lhs, llvm::Value * rhs ) noexcept;

    void propagate_back( llvm::Argument * arg ) noexcept;

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
    struct Onion : std::enable_shared_from_this< Onion< Core > >
    {
        using Layer = std::shared_ptr< Onion >;
        using Ptr = std::shared_ptr< Onion >;
        using ConstPtr = std::shared_ptr< const Onion >;

        Ptr getptr()
        {
            return this->shared_from_this();
        }

        ConstPtr getptr() const
        {
            return this->shared_from_this();
        }

        bool is_covered() const
        {
            return std::holds_alternative< Layer >( layer );
        }

        bool is_core() const
        {
            return std::holds_alternative< Core >( layer );
        }

        void to_bottom()
        {
            ASSERT( is_core() );
            std::get< Core >( layer ).to_bottom();
        }

        void to_top()
        {
            ASSERT( is_core() );
            std::get< Core >( layer ).to_top();
        }

        bool join( const Ptr &other )
        {
            // TODO peel to the same level
            return core().join( other->core() );
        }

        Ptr peel() const noexcept
        {
            ASSERT( is_covered() );
            return (*std::get< Layer >( layer )).getptr();
        }

        Ptr cover() noexcept
        {
            auto on = std::make_shared< Onion >();
            on->layer = getptr();
            return on;
        }

        Core& core() noexcept
        {
            auto ptr = getptr();
            while ( !ptr->is_core() )
                ptr = ptr->peel();
            return std::get< Core >( ptr->layer );
        }

        const Core& core() const noexcept
        {
            auto ptr = getptr();
            while ( !ptr->is_core() )
                ptr = ptr->peel();
            return std::get< Core >( ptr->layer );
        }

        std::variant< Layer, Core > layer = Core{};
    };

    template< typename I >
    struct IntervalMap
    {
        using Ptr = std::shared_ptr< I >;

        bool has( llvm::Value * val ) const noexcept
        {
            return _intervals.count( val );
        }

        Ptr& operator[]( llvm::Value * val ) noexcept
        {
            if ( !has( val ) ) {
                _intervals[ val ] = std::make_shared< I >();
                _intervals[ val ]->to_bottom();
            }
            return _intervals[ val ];
        }

        decltype(auto) begin() { return _intervals.begin(); }
        decltype(auto) begin() const { return _intervals.begin(); }

        decltype(auto) end() { return _intervals.end(); }
        decltype(auto) end() const { return _intervals.end(); }

        std::map< llvm::Value *, Ptr > _intervals;
    };

    inline auto& interval( llvm::Value * val ) noexcept
    {
        return _intervals[ val ];
    }

    using MapValue = Onion< LatticeValue >;
    using MapValuePtr = IntervalMap< MapValue >::Ptr;

    bool visited( llvm::Value * val ) const noexcept;
    bool propagate( llvm::Value * to, const MapValuePtr& from ) noexcept;

    IntervalMap< MapValue > _intervals;

    void add_meta( llvm::Value *value, const MapValuePtr& mval ) noexcept;

    std::deque< Task > _tasks;
};

} // namespace lart::abstract
