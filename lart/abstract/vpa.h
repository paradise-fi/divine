// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/InstVisitor.h>
DIVINE_UNRELAX_WARNINGS

#include <deque>
#include <set>
#include <unordered_set>

#include <lart/abstract/domain.h>

namespace lart::abstract {

struct VPA {
    using Task = std::function< void() >;

    void run( llvm::Module & );

    void propagate( llvm::Value *inst ) noexcept;

    bool join( llvm::Value *, llvm::Value * ) noexcept;

    inline void push( Task && t ) noexcept {
        _tasks.emplace_back( std::move( t ) );
    }

    struct LatticeValue {

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

        bool join( const LatticeValue & /*other*/ ) noexcept
        {
            if ( is_top( value ) )
                return false;
            to_top();
            return true;
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
            if ( !has( val ) )
                _intervals[ val ].to_bottom();
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

    IntervalMap< LatticeValue > _intervals;

    std::deque< Task > _tasks;
};

} // namespace lart::abstract
