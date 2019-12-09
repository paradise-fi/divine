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
            abstract_value_t size; // index_t
        };

        index_t size() const noexcept { return data->size; }
        auto& bounds() noexcept { return data->bounds; }
        const auto& bounds() const noexcept { return data->bounds; }
        auto& values() noexcept { return data->values; }
        const auto& values() const noexcept { return data->values; }

        using data_ptr = brq::refcount_ptr< data_t >;

        struct mstring_ptr
        {
            mstring_ptr( mstring_t * p ) : ptr( p ) {}
            mstring_ptr( abstract_value_t a )
                : mstring_ptr( static_cast< mstring_t * >( a ) ) {}

            mstring_t *operator->() const noexcept { return ptr; }
            mstring_t &operator*() const noexcept { return *ptr; }

            auto data() noexcept { return ptr->data; }
            index_t offset() noexcept { return ptr->offset; }

            index_t size() const noexcept { return ptr->size(); }
            auto& bounds() noexcept { return ptr->bounds(); }
            const auto& bounds() const noexcept { return ptr->bounds(); }
            auto& values() noexcept { return ptr->values(); }
            const auto& values() const noexcept { return ptr->values(); }

            operator abstract_value_t()
            {
                return static_cast< abstract_value_t >( ptr );
            }

            void push_bound( abstract_value_t bound ) noexcept
            {
                bounds().push_back( bound );
            }

            void push_bound( unsigned bound ) noexcept
            {
                push_bound( constant_t::lift( bound ) );
            }

            void push_char( abstract_value_t ch ) noexcept
            {
                values().push_back( ch );
            }

            void push_char( char ch ) noexcept
            {
                push_char( constant_t::lift( ch ) );
            }

            mstring_t * ptr;
        };

        abstract_value_t offset;
        data_ptr data;

        _LART_INLINE
        static mstring_ptr make_mstring( data_ptr data, abstract_value_t offset ) noexcept
        {
            auto ptr = __new< mstring_t >();
            ptr->offset = offset;
            ptr->data = data;
            return ptr;
        }

        _LART_INLINE
        static mstring_ptr make_mstring( abstract_value_t size ) noexcept
        {
            auto data = brq::make_refcount< data_t >();
            data->size = size;

            return make_mstring( data, constant_t::lift( 0 ) );
        }

        _LART_INLINE
        static mstring_ptr make_mstring( unsigned size ) noexcept
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
        static character_t op_load( mstring_ptr array, bitwidth_t bw ) noexcept
        {
            if ( bw != 8 )
                __dios_fault( _VM_Fault::_VM_F_Assert, "unexpected store bitwidth" );

            // TODO assert array is mstring
            return load( array );
        }

        _LART_INTERFACE _LART_OPTNONE
        static void op_store( abstract_value_t value, abstract_value_t array, bitwidth_t bw ) noexcept
        {
            if ( bw != 8 )
                __dios_fault( _VM_Fault::_VM_F_Assert, "unexpected store bitwidth" );

            // TODO assert array is mstring
            return store( value, array );
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

        _LART_INTERFACE
        static character_t load( mstring_ptr array ) noexcept
        {
            if ( static_cast< bool >( array.offset() >= array.size() ) )
                __dios_fault( _VM_Fault::_VM_F_Memory, "Access out of bounds." );

            return array->segment_at_current_offset().value();
        }

        _LART_INLINE
        static void store( character_t ch, mstring_ptr array ) noexcept
        {
            auto offset = array.offset();
            if ( static_cast< bool >( offset >= array.size() ) )
                __dios_fault( _VM_Fault::_VM_F_Memory, "Access out of bounds." );

            auto one = index_t::lift( 1 );

            auto& bounds = array.bounds();
            auto& values = array.values();

            auto seg = array->segment_at_current_offset();
            if ( seg.value() == ch ) {
                __dios_trace_f( "do nothing" );
            } else if ( seg.singleton() ) {
                // rewrite single character segment
                seg.set_char( ch );
            } else if ( seg.begin() == offset ) {
                // rewrite first character of segment
                if ( seg.begin() != bounds.begin() ) {
                    auto prev = --seg;
                    if ( prev.value() == ch ) {
                        // merge with left neighbour
                        seg.begin() = seg.begin() + one;
                        return;
                    }
                }

                bounds.insert( seg.end_it(), offset + one );
                values.insert( seg.val_it(), ch );
            } else if ( seg.end() == offset ) {
                // rewrite last character of segment
                if ( seg.end() != std::prev( bounds.end() ) ) {
                    auto next = ++seg;
                    if ( next.value() == ch ) {
                        // merge with left neighbour
                        seg.end() = seg.end() - one;
                        return;
                    }
                }
                bounds.insert( seg.end_it(), offset );
                values.insert( std::next( seg.val_it() ), ch );
            } else {
                // rewrite segment in the middle (split segment)
                auto vit = values.insert( seg.val_it(), seg.value() );
                values.insert( std::next( vit ), ch );
                auto bit = bounds.insert( seg.end_it(), offset );
                bounds.insert( std::next( bit ), offset + one );
            }
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

        _LART_INLINE
        size_t concrete_offset() const noexcept
        {
            return static_cast< index_t >( offset ).template lower< size_t >();
        }

        _LART_INLINE
        size_t concrete_size() const noexcept
        {
            return static_cast< index_t >( size() ).template lower< size_t >();
        }

        _LART_INLINE
        friend void trace( mstring_t &mstr ) noexcept
        {
           __dios_trace_f( "mstring offset %lu size %lu:", mstr.concrete_offset(), mstr.concrete_size() );
           auto seg = mstr.segment_at_current_offset();
           while ( seg.end_it() != mstr.bounds().end() ) {
               trace( seg );
               ++seg;
           }
        }

        /* detail */

        struct segment_t
        {
            bounds_iterator _begin;
            values_iterator _value;

            auto begin_it() noexcept { return _begin; }
            auto begin_it() const noexcept { return _begin; }
            auto end_it() noexcept { return std::next( _begin ); }
            auto end_it() const noexcept { return std::next( _begin ); }
            auto val_it() noexcept { return _value; }
            auto val_it() const noexcept { return _value; }

            index_t& begin() const noexcept { return *begin_it(); }
            index_t& end() const noexcept { return *end_it(); }
            character_t& value() const noexcept { return *_value; }

            void set_char( character_t ch ) noexcept { *_value = ch; }

            segment_t& operator++() noexcept
            {
                ++_begin;
                ++_value;
                return *this;
            }

            segment_t& operator--() noexcept
            {
                --_begin; --_value;
                return *this;
            }

            bool empty() const noexcept { return begin() == end(); }

            bool singleton() const noexcept
            {
                return begin() + index_t::lift( 1 ) == end();
            }

            friend void trace( const segment_t &seg ) noexcept
            {
                __dios_trace_f( "seg [%lu, %lu]: %c", seg.begin().template lower< size_t >()
                                                    , seg.end().template lower< size_t >()
                                                    , seg.value().template lower< char >() );
            }
        };

        _LART_INLINE
        segment_t segment_at_index( index_t idx ) noexcept
        {
            auto it = bounds().begin();

            for ( auto it = bounds().begin(); std::next( it ) != bounds().end(); ++it )
                if ( idx >= *it && idx < *std::next( it ) ) {
                    auto nth = std::distance( bounds().begin(), it );
                    return segment_t{ it, std::next( values().begin(), nth ) };
                }

            UNREACHABLE( "MSTRIG ERROR: index out of bounds" );
        }

        _LART_INLINE
        segment_t segment_at_current_offset() noexcept
        {
            return segment_at_index( offset );
        }
    };

} // namespace __dios::rst::abstract
