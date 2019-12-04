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
        };

        using data_ptr = brq::refcount_ptr< data_t >;

        using mstring_ptr = abstract_object< data_ptr, index_t >;

        volatile uint32_t size_objid;  // index_t
        volatile uint32_t array_objid; // data_ptr

        index_t size() noexcept { return object_pointer( size_objid ); }
        data_ptr array() noexcept { return object_pointer( array_objid ); }

        _LART_INTERFACE _LART_OPTNONE
        static mstring_ptr lift_any( index_t size ) noexcept
        {
            UNREACHABLE( "not implemented" );
        }

        _LART_INTERFACE _LART_OPTNONE
        static mstring_ptr lift_any( unsigned size ) noexcept
        {
            UNREACHABLE( "not implemented" );
        }

        _LART_INTERFACE _LART_OPTNONE
        static mstring_ptr lift_one( const char * str, unsigned size ) noexcept
        {
            UNREACHABLE( "not implemented" );
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