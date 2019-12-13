// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <rst/base.hpp>
#include <rst/scalar.hpp>

#include <util/array.hpp>
#include <brick-ptr>

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

        template< _VM_Fault fault_v = _VM_Fault::_VM_F_Assert >
        static void fault( const char * msg ) noexcept
        {
            __dios_fault( fault_v, msg );
        }

        static void out_of_bounds_fault() noexcept
        {
            fault< _VM_Fault::_VM_F_Memory >( "Access out of bounds." );
        }

        struct data_t : brq::refcount_base< uint16_t, false /* atomic */ >
        {
            static void* operator new( size_t s ) noexcept { return __vm_obj_make( s, _VM_PT_Heap ); }
            static void operator delete( void *p ) noexcept { return __vm_obj_free( p ); }

            values_t values;
            bounds_t bounds;
            abstract_value_t size; // index_t
        };

        index_t size() const noexcept { return _data->size; }
        auto& bounds() noexcept { return _data->bounds; }
        const auto& bounds() const noexcept { return _data->bounds; }
        auto& values() noexcept { return _data->values; }
        const auto& values() const noexcept { return _data->values; }

        using data_ptr = brq::refcount_ptr< data_t >;

        struct mstring_ptr
        {
            mstring_ptr( mstring_t * p ) : ptr( p ) {}
            mstring_ptr( abstract_value_t a )
                : mstring_ptr( static_cast< mstring_t * >( a ) ) {}

            mstring_t *operator->() const noexcept { return ptr; }
            mstring_t &operator*() const noexcept { return *ptr; }

            auto& data() noexcept { return ptr->_data; }
            index_t size() const noexcept { return ptr->size(); }
            index_t offset() noexcept { return ptr->_offset; }
            index_t checked_offset() noexcept
            {
                auto off = offset();
                if ( static_cast< bool >( off >= size() ) )
                    out_of_bounds_fault();
                return off;
            }

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

            index_t terminator() noexcept
            {
                return ptr->terminator();
            }

            _LART_INLINE
            friend void trace( const mstring_ptr &mstr ) noexcept { trace( *mstr ); }

            mstring_t * ptr;
        };

        abstract_value_t _offset;
        data_ptr _data;

        _LART_INLINE
        static mstring_ptr make_mstring( data_ptr data, abstract_value_t offset ) noexcept
        {
            auto ptr = __new< mstring_t >();
            ptr->_offset = offset;
            ptr->_data = data;
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
                fault( "unexpected store bitwidth" );

            // TODO assert array is mstring
            return load( array );
        }

        _LART_INTERFACE _LART_OPTNONE
        static void op_store( abstract_value_t value, abstract_value_t array, bitwidth_t bw ) noexcept
        {
            if ( bw != 8 )
                fault( "unexpected store bitwidth" );

            // TODO assert array is mstring
            auto str = static_cast< mstring_ptr >( array );
            str->store( value, str.checked_offset() );
        }

        _LART_INTERFACE _LART_OPTNONE
        static mstring_ptr op_gep( size_t bw, abstract_value_t array, abstract_value_t idx ) noexcept
        {
            if ( bw != 8 )
                fault( "unexpected gep bitwidth" );

            // TODO assert array is mstring
            return gep( array, idx );
        }

        _LART_INTERFACE _LART_SCALAR
        static index_t fn_strlen( mstring_ptr str ) noexcept
        {
            return strlen( str );
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

        struct segment_t
        {
            bounds_iterator _begin;
            values_iterator _value;

            auto begin() noexcept { return _begin; }
            auto begin() const noexcept { return _begin; }
            auto end() noexcept { return std::next( _begin ); }
            auto end() const noexcept { return std::next( _begin ); }
            auto val_it() noexcept { return _value; }
            auto val_it() const noexcept { return _value; }

            index_t& from() const noexcept { return *begin(); }
            index_t& to() const noexcept { return *end(); }
            character_t& value() const noexcept { return *_value; }

            void set_char( character_t ch ) noexcept { *_value = ch; }

            character_t& operator*() & { return *_value; }
            character_t& operator*() && = delete;
            character_t* operator->() const noexcept { return _value; }

            inline segment_t& operator+=( const int& rhs )
            {
                _begin += rhs;
                _value += rhs;
                return *this;
            }

            inline segment_t& operator-=( const int& rhs )
            {
                _begin -= rhs;
                _value -= rhs;
                return *this;
            }

            segment_t& operator++() noexcept
            {
                ++_begin;
                ++_value;
                return *this;
            }

            segment_t operator++(int) noexcept
            {
                segment_t tmp(*this);
                operator++();
                return tmp;
            }

            segment_t& operator--() noexcept
            {
                --_begin; --_value;
                return *this;
            }

            segment_t operator--(int) noexcept
            {
                segment_t tmp(*this);
                operator++();
                return tmp;
            }

            bool empty() const noexcept { return from() == to(); }

            bool singleton() const noexcept
            {
                return from() + index_t::lift( 1 ) == to();
            }

            friend void trace( const segment_t &seg ) noexcept
            {
                __dios_trace_f( "seg [%lu, %lu]: %c", seg.from().template lower< size_t >()
                                                    , seg.to().template lower< size_t >()
                                                    , seg.value().template lower< char >() );
            }
        };

        struct segments_range
        {
            template< typename iterator_t >
            struct range_t
            {
                iterator_t from;
                iterator_t to;

                auto begin() { return from; }
                auto end() { return to; }
            };

            bool single_segment() noexcept { return bounds.to == std::next( bounds.from ); }

            segment_t begin() { return { bounds.begin(), values.begin() }; }
            segment_t end() { return { bounds.end(), values.end() }; }

            _LART_INLINE
            segment_t next_segment( segment_t seg ) noexcept
            {
                do { ++seg; } while ( seg.begin() != bounds.end() && seg.empty() );
                return seg;
            }

            range_t< bounds_iterator > bounds;
            range_t< values_iterator > values;
        };

        _LART_INLINE
        segments_range range( segment_t f, segment_t t ) noexcept
        {
            return { { f.begin(), t.end() }, { f.val_it(), t.val_it() } };
        }

        _LART_INLINE
        segments_range range( index_t from, index_t to ) noexcept
        {
            auto f = segment_at_index( from );
            auto t = segment_at_index( to );
            return range( f, t );
        }

        _LART_INLINE
        segments_range interest() noexcept
        {
            return range( segment_at_current_offset(), terminal_segment() );
        }

        _LART_INTERFACE
        static character_t load( mstring_ptr array ) noexcept
        {
            return array->segment_at_current_offset().value();
        }

        _LART_INLINE
        static void store( character_t ch, mstring_ptr array ) noexcept
        {
            auto offset = array.checked_offset();
            auto one = index_t::lift( 1 );

            auto& bounds = array.bounds();
            auto& values = array.values();

            auto seg = array->segment_at_current_offset();
            if ( seg.value() == ch ) {
                // do nothing
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
        static mstring_ptr gep( mstring_ptr array, index_t idx ) noexcept
        {
            return make_mstring( array.data(), array.offset() + idx );
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
            if ( static_cast< bool >( idx >= size() ) )
                out_of_bounds_fault();

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
