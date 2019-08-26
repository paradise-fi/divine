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

namespace lart::abstract
{
    struct tristate : brick::types::Eq
    {
        enum { no, yes, maybe } value;
        tristate( bool v ) : value( v ? yes : no ) {}
        tristate( decltype( value ) v ) : value( v ) {}
        bool operator==( const tristate &o ) const { return value == o.value; }

        template< typename stream >
        friend auto operator<<( stream &s, tristate t ) -> decltype( s << "" )
        {
            switch ( t.value )
            {
                case no: return s << "no";
                case yes: return s << "yes";
                case maybe: return s << "maybe";
            }
        }
    };

    inline tristate join( tristate a, tristate b )
    {
        if ( a.value == b.value )
            return a;
        else
            return tristate( tristate::maybe );
    }

    struct type_layer : brick::types::Eq
    {
        tristate is_pointer;
        tristate is_abstract;
        type_layer( bool p, bool a ) : is_pointer( p ), is_abstract( a ) {}
        type_layer( tristate p, tristate a ) : is_pointer( p ), is_abstract( a ) {}

        bool operator==( const type_layer &o ) const
        {
            return is_pointer == o.is_pointer && is_abstract == o.is_abstract;
        }

        template< typename stream >
        friend auto operator<<( stream &s, type_layer t ) -> decltype( s << "" )
        {
            return s << "ptr:" << t.is_pointer << " abs:" << t.is_abstract;
        }
    };

    inline type_layer join( type_layer a, type_layer b )
    {
        return { join( a.is_pointer, b.is_pointer ),
                 join( a.is_abstract, b.is_abstract ) };
    }

    using type_vector = std::vector< type_layer >;

    struct type_onion : type_vector
    {
        type_onion( int ptr_nest )
            : type_vector( ptr_nest + 1, type_layer( true, false ) )
        {
            front() = type_layer( false, false );
        }

        type_onion( const type_vector &l ) : type_vector( l ) {}
        type_onion( std::initializer_list< type_layer > il ) : type_vector( il ) {}

        type_onion make_abstract() const
        {
            auto rv = *this;
            rv.back().is_abstract = true;
            return rv;
        }

        bool maybe_abstract() const
        {
            tristate r( false );
            for ( auto a : *this )
                r = join( r, a.is_abstract );
            return r.value != tristate::no;
        }

        bool maybe_pointer() const
        {
            tristate r( false );
            for ( auto a : *this )
                r = join( r, a.is_pointer );
            return r.value != tristate::no;
        }

        type_onion wrap() const
        {
            auto rv = *this;
            rv.emplace_back( true, false );
            return rv;
        }

        type_onion peel() const
        {
            auto rv = *this;
            if ( size() == 1 )
                rv.front().is_pointer = tristate::maybe;
            else
            {
                ASSERT_NEQ( back().is_pointer, tristate( false ) );
                rv.pop_back();
            }
            return rv;
        }
    };

    inline type_onion join( type_onion a, type_onion b )
    {
        if ( a.size() > b.size() )
            std::swap( a, b );

        if ( a.size() < b.size() )
        {
            for ( size_t i = 0; i < b.size() - a.size(); ++i )
                a.front() = join( a.front(), b[ i ] );
            a.front().is_pointer = tristate::maybe;
        }

        for ( size_t i = 0; i < a.size(); ++i )
            a[ i ] = join( a[ i ], b[ i + b.size() - a.size() ] );

        return a;
    }

    struct type_map
    {
        int pointer_nesting( llvm::Type *t )
        {
            int rv = 0;
            while ( t->isPointerTy() )
                ++ rv, t = t->getPointerElementType();
            return rv;
        }

        type_onion get( llvm::Value *v )
        {
            if ( _map.count( v ) )
                return _map.at( v );
            else
                return type_onion( pointer_nesting( v->getType() ) );
        }

        void set( llvm::Value *v, type_onion o )
        {
            if ( auto it = _map.find( v ); it != _map.end() )
                it->second = o;
            else
                _map.emplace( v, o );
        }

        decltype(auto) begin() { return _map.begin(); }
        decltype(auto) begin() const { return _map.begin(); }

        decltype(auto) end() { return _map.end(); }
        decltype(auto) end() const { return _map.end(); }

        std::map< llvm::Value *, type_onion > _map;
    };

struct DataFlowAnalysis
{
    using Task = std::function< void() >;

    void run( llvm::Module & );

    void preprocess( llvm::Function * fn ) noexcept;

    void propagate( llvm::Value * inst ) noexcept;

    void propagate_in( llvm::Function *fn, llvm::CallSite call ) noexcept;
    void propagate_out( llvm::ReturnInst * ret ) noexcept;

    bool propagate_wrap( llvm::Value * lhs, llvm::Value * rhs ) noexcept;
    bool propagate_identity( llvm::Value * lhs, llvm::Value * rhs ) noexcept;

    void propagate_back( llvm::Argument * arg ) noexcept;

    inline void push( llvm::Value *v ) noexcept
    {
        TRACE( "push", _todo.size(), *v );
        if ( !_queued.count( v ) && !_blocked.count( v ) )
        {
            _queued.emplace( v );
            _blocked.emplace( v );
            _todo.push_back( v );
        }
    }

    bool propagate( llvm::Value *to, const type_onion& from ) noexcept;
    void add_meta( llvm::Value *value, const type_onion& t ) noexcept;

    type_map _types;
    std::deque< llvm::Value * > _todo;
    std::set< llvm::Value * > _queued, _blocked;
};

} // namespace lart::abstract
