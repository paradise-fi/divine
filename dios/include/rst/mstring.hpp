// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <rst/base.hpp>
#include <rst/scalar.hpp>

#include <util/array.hpp>
#include <bricks/brick-ptr>

namespace __dios::rst::abstract {

    template< typename Index, typename Char >
    struct mstring_t : tagged_abstract_domain_t
    {
        using index_t = scalar_t< Index >;
        using character_t = scalar_t< Char >;

        using values_t = Array< character_t >;
        using values_iterator = typename values_t::iterator;

        using bounds_t = Array< index_t >;
        using bounds_iterator = typename bounds_t::iterator;

        struct data_t : brq::refcount_base< uint16_t, false /* atomic */ >
        {
            values_t values;
            bounds_t bounds;
            abstract_value_t size; // either< index_t, constant_t >
        };

        using data_ptr = brq::refcount_ptr< data_t >;

        struct mstring_ptr
        {
            mstring_ptr( mstring_t * p ) : ptr( p ) {}

            auto& bounds() noexcept { return ptr->data->bounds; }
            auto& values() noexcept { return ptr->data->values; }

            operator abstract_value_t()
            {
                return static_cast< abstract_value_t >( ptr );
            }

            void push_bound( abstract_value_t bound )
            {
                bounds().push_back( bound );
            }

            void push_bound( unsigned bound ) noexcept
            {
                push_bound( constant_t::lift( bound ) );
            }

            void push_char( abstract_value_t ch )
            {
                values().push_back( ch );
            }

            void push_char( char ch ) noexcept
            {
                push_bound( constant_t::lift( ch ) );
            }

            mstring_t * ptr;
        };

        abstract_value_t offset;
        data_ptr data;

        _LART_INLINE
        static mstring_ptr make_mstring( abstract_value_t size )
        {
            auto data = brq::make_refcount< data_t >();
            data->size = size;

            auto ptr = __new< mstring_t >();
            ptr->offset = constant_t::lift( 0 );
            ptr->data = data;
            return ptr;
        }

        _LART_INLINE
        static mstring_ptr make_mstring( unsigned size )
        {
            return make_mstring( constant_t::lift( size ) );
        }

        _LART_INTERFACE _LART_OPTNONE
        static mstring_ptr lift_any( index_t size ) noexcept
        {
            return make_mstring( size );
        }

        _LART_INTERFACE _LART_OPTNONE
        static mstring_ptr lift_any( unsigned size ) noexcept
        {
            return make_mstring( size );
        }

        _LART_INTERFACE
        static mstring_ptr lift_one( const char * str, unsigned size ) noexcept
        {
            auto mstr = make_mstring( size );
            mstr.push_bound( unsigned( 0 ) );

            if ( size == 0 )
                return mstr;

            char prev = str[ 0 ];
            for ( int i = 1; i < size; ++i ) {
                if ( prev != str[ i ] ) {
                    mstr.push_char( prev );
                    mstr.push_bound( i );
                    prev = str[ i ];
                }
            }

            mstr.push_char( prev );
            mstr.push_bound( size );

            return mstr;
        }

        _LART_INTERFACE _LART_OPTNONE
        static mstring_ptr lift_one( const char * str ) noexcept
        {
            return lift_one( str, __vm_obj_size( str ) );
        }

        _LART_INTERFACE _LART_OPTNONE
        static character_t op_load( mstring_ptr /* ptr */, bitwidth_t /* bw */ ) noexcept
        {
            UNREACHABLE( "not implemented" );
        }

        _LART_INTERFACE _LART_OPTNONE
        static void op_store( abstract_value_t /*value*/, abstract_value_t /*array*/, bitwidth_t /*bw*/ ) noexcept
        {
            UNREACHABLE( "not implemented" );
        }

        _LART_INTERFACE _LART_OPTNONE
        static mstring_ptr op_gep( size_t /*base*/, abstract_value_t /*array*/
                                                  , abstract_value_t /*idx*/ ) noexcept
        {
            UNREACHABLE( "not implemented" );
        }

        _LART_INTERFACE _LART_SCALAR
        static index_t fn_strlen( mstring_ptr str ) noexcept
        {
            UNREACHABLE( "not implemented" );
        }

        _LART_INTERFACE _LART_SCALAR
        static character_t fn_strcmp( abstract_value_t lhs, abstract_value_t rhs ) noexcept
        {
            UNREACHABLE( "not implemented" );
        }

        _LART_INTERFACE _LART_AGGREGATE
        static mstring_ptr fn_strcat( abstract_value_t lhs, abstract_value_t rhs ) noexcept
        {
            UNREACHABLE( "not implemented" );
        }

        _LART_INTERFACE _LART_AGGREGATE
        static mstring_ptr fn_strcpy( abstract_value_t lhs, abstract_value_t rhs ) noexcept
        {
            UNREACHABLE( "not implemented" );
        }

        _LART_INTERFACE _LART_AGGREGATE
        static mstring_ptr fn_strchr( abstract_value_t str, abstract_value_t ch ) noexcept
        {
            UNREACHABLE( "not implemented" );
        }

        /* implementation of abstraction interface */

        _LART_INLINE
        static void store( character_t /*value*/, mstring_ptr /*array*/, bitwidth_t /*bw*/ ) noexcept
        {
            UNREACHABLE( "not implemented" );
        }

        _LART_INLINE
        static mstring_ptr gep( size_t /*base*/, mstring_ptr /*array*/, index_t /*idx*/ ) noexcept
        {
            UNREACHABLE( "not implemented" );
        }

        _LART_INLINE
        static index_t strlen( mstring_ptr str ) noexcept
        {
            UNREACHABLE( "not implemented" );
        }

        _LART_INLINE
        static character_t strcmp( mstring_ptr lhs, mstring_ptr rhs ) noexcept
        {
            UNREACHABLE( "not implemented" );
        }

        _LART_INLINE
        static mstring_ptr strcat( mstring_ptr lhs, mstring_ptr rhs ) noexcept
        {
            UNREACHABLE( "not implemented" );
        }

        _LART_INLINE
        static mstring_ptr strcpy( mstring_ptr lhs, mstring_ptr rhs ) noexcept
        {
            UNREACHABLE( "not implemented" );
        }

        _LART_INLINE
        static mstring_ptr strhr( mstring_ptr str, character_t ch ) noexcept
        {
            UNREACHABLE( "not implemented" );
        }
    };

} // namespace __dios::rst::abstract