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

        using values_t = Array< character_t, _VM_PT_Weak >;
        using values_iterator = typename values_t::iterator;

        using bounds_t = Array< index_t, _VM_PT_Weak >;
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

            _LART_NOINLINE
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

            void push_bound( uint64_t bound ) noexcept
            {
                push_bound( static_cast< abstract_value_t >( index_t::lift( bound ) ) );
            }

            void push_char( abstract_value_t ch ) noexcept
            {
                values().push_back( ch );
            }

            void push_char( char ch ) noexcept
            {
                push_char( static_cast< abstract_value_t >( character_t::lift( ch ) ) );
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
            auto ptr = __new< mstring_t, _VM_PT_Weak >();
            ptr->_offset = offset;
            ptr->_data = data;
            return ptr;
        }

        _LART_INLINE
        static mstring_ptr make_mstring( abstract_value_t size ) noexcept
        {
            auto data = brq::make_refcount< data_t >();
            data->size = size;

            return make_mstring( data, static_cast< abstract_value_t >( index_t::lift( uint64_t( 0 ) ) ) );
        }

        _LART_INLINE
        static mstring_ptr make_mstring( uint64_t size ) noexcept
        {
            return make_mstring( static_cast< abstract_value_t >( index_t::lift( size ) ) );
        }

        _LART_INTERFACE
        static mstring_ptr lift_any( index_t size ) noexcept
        {
            return make_mstring( size );
        }

        _LART_INTERFACE
        static mstring_ptr lift_any( uint64_t size ) noexcept
        {
            return make_mstring( size );
        }

        _LART_INTERFACE
        static mstring_ptr lift_one( const char * str, uint64_t size ) noexcept
        {
            auto mstr = make_mstring( size );
            mstr.push_bound( uint64_t( 0 ) );

            if ( size == 0 )
                return mstr;

            char prev = str[ 0 ];
            for ( uint64_t i = 1; i < size; ++i ) {
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

        _LART_INTERFACE
        static mstring_ptr lift_one( const char * str ) noexcept
        {
            return lift_one( str, (uint64_t)__vm_obj_size( str ) );
        }

        _LART_INTERFACE
        static character_t op_load( mstring_ptr array, bitwidth_t bw ) noexcept
        {
            if ( bw != 8 )
                fault( "unexpected store bitwidth" );

            // TODO assert array is mstring
            return load( array );
        }

        _LART_INTERFACE
        static void op_store( abstract_value_t value, abstract_value_t array, bitwidth_t bw ) noexcept
        {
            if ( bw != 8 )
                fault( "unexpected store bitwidth" );

            if ( is_constant( value ) )
                value = character_t::lift(
                    constant_t::get_constant( value ).lower< char >()
                );
            // TODO assert array is mstring
            auto str = static_cast< mstring_ptr >( array );
            str->store( value, str.checked_offset() );
        }

        _LART_INTERFACE
        static mstring_ptr op_gep( size_t bw, abstract_value_t array, abstract_value_t idx ) noexcept
        {
            if ( bw != 8 )
                fault( "unexpected gep bitwidth" );

            if ( is_constant( idx ) )
                idx = index_t::lift(
                    constant_t::get_constant( idx ).lower< uint64_t >()
                );
            // TODO assert array is mstring
            return gep( array, idx );
        }

        _LART_INTERFACE
        static mstring_ptr op_thaw( mstring_ptr str, uint8_t bw ) noexcept
        {
            return str;
        }

        _LART_INTERFACE _LART_SCALAR _LART_OPTNONE
        static index_t fn_strlen( mstring_ptr str ) noexcept
        {
            return strlen( str );
        }

        _LART_INTERFACE _LART_SCALAR
        static character_t fn_strcmp( abstract_value_t lhs, abstract_value_t rhs ) noexcept
        {
            return character_t::template zfit< 32 >( strcmp( lhs, rhs ) );
        }

        _LART_INTERFACE _LART_AGGREGATE
        static mstring_ptr fn_strcat( abstract_value_t dst, abstract_value_t src ) noexcept
        {
            // TODO lift strings
            return strcat( dst, src );
        }

        _LART_INTERFACE _LART_AGGREGATE
        static mstring_ptr fn_strcpy( abstract_value_t dst, abstract_value_t src ) noexcept
        {
            // TODO lift strings
            return strcpy( dst, src );
        }

        _LART_INTERFACE _LART_AGGREGATE
        static mstring_ptr fn_strchr( abstract_value_t str, abstract_value_t ch ) noexcept
        {
            // TODO lift strings
            if ( is_constant( ch ) )
                ch = character_t::lift(
                    constant_t::get_constant( ch ).lower< char >()
                );
            return strchr( str, ch );
        }

        _LART_INTERFACE _LART_AGGREGATE
        static mstring_ptr fn_memcpy( abstract_value_t dst
                                    , abstract_value_t src
                                    , abstract_value_t size ) noexcept
        {
            // TODO lift strings
            if ( is_constant( size ) )
                size = index_t::lift(
                    constant_t::get_constant( size ).lower< size_t >()
                );
            return memcpy( dst, src, size );
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
                return from() + index_t::lift( uint64_t( 1 ) ) == to();
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
        void store( character_t ch, index_t idx ) noexcept
        {
            auto one = index_t::lift( uint64_t( 1 ) );
            auto seg = segment_at_index( idx );
            if ( seg.value() == ch ) {
                // do nothing
            } else if ( seg.singleton() ) {
                // rewrite single character segment
                seg.set_char( ch );
            } else if ( seg.from() == idx ) {
                // rewrite first character of segment
                if ( seg.begin() != bounds().begin() ) {
                    auto prev = seg; --prev;
                    if ( prev.value() == ch ) {
                        // merge with left neighbour
                        seg.from() = seg.from() + one;
                        return;
                    }
                }

                bounds().insert( seg.end(), idx + one );
                values().insert( seg.val_it(), ch );
            } else if ( seg.to() - one == idx ) {
                // rewrite last character of segment
                if ( seg.end() != std::prev( bounds().end() ) ) {
                    auto next = seg; ++next;
                    if ( next.value() == ch ) {
                        // merge with left neighbour
                        seg.to() = seg.to() - one;
                        return;
                    }
                }
                bounds().insert( seg.end(), idx );
                values().insert( std::next( seg.val_it() ), ch );
            } else {
                // rewrite segment in the middle (split segment)
                auto vit = values().insert( seg.val_it(), seg.value() );
                values().insert( std::next( vit ), ch );
                auto bit = bounds().insert( seg.end(), idx );
                bounds().insert( std::next( bit ), idx + one );
            }
        }

        _LART_INLINE
        static mstring_ptr gep( mstring_ptr array, index_t idx ) noexcept
        {
            return make_mstring( array.data(), array.offset() + idx );
        }

        _LART_INLINE
        static index_t strlen( mstring_ptr str ) noexcept
        {
            auto res = (str.terminator() - str.offset());
            return index_t::template zfit< 64 >( res );
        }

        _LART_INLINE
        static character_t strcmp( mstring_ptr lhs, mstring_ptr rhs ) noexcept
        {
            auto lin = lhs->interest();
            auto rin = rhs->interest();

            auto lseg = lin.begin();
            auto rseg = rin.begin();

            while ( lseg.begin() != lin.bounds.end() && rseg.begin() != rin.bounds.end() ) {
                if ( lseg.value() != rseg.value() ) {
                    return lseg.value() - rseg.value();
                } else {
                    index_t left = lseg.to() - lhs.offset();
                    index_t right = rseg.to() - rhs.offset();
                    if ( static_cast< bool >( left > right ) ) {
                        return lseg.value() - rin.next_segment( lseg ).value();
                    } else if ( static_cast< bool >( left < right ) ) {
                        return lin.next_segment( lseg ).value() - rseg.value();
                    }
                }

                lseg = lin.next_segment( lseg );
                rseg = rin.next_segment( rseg );
            }

            return character_t::lift( '\0' );
        }

        _LART_INLINE
        static mstring_ptr strcat( mstring_ptr dst, mstring_ptr src ) noexcept
        {
            // TODO optimize
            auto off = dst.offset();
            auto size = src.terminator() - src.offset();
            dst->_offset = dst.terminator();
            auto ret = memcpy( dst, src, size );
            ret->_offset = off;
            return ret;
        }

        _LART_INLINE
        static mstring_ptr strcpy( mstring_ptr dst, mstring_ptr src ) noexcept
        {
            // TODO optimize
            return memcpy( dst, src, strlen( src ) + index_t::lift( size_t( 1 ) ) );
        }

        _LART_INLINE
        static mstring_ptr strchr( mstring_ptr str, character_t ch ) noexcept
        {
            auto in = str->interest();
            auto seg = in.begin();
            while ( seg.begin() != in.bounds.end() ) {
                if ( !seg.empty() && seg.value() == ch ) {
                    auto off = seg.from();
                    if ( seg.begin() == in.bounds.begin() )
                        return str;
                    return make_mstring( str.data(), off );
                }
                ++seg;
            }

            return static_cast< mstring_t * >( nullptr );
        }

        _LART_NOINLINE
        static mstring_ptr memcpy( mstring_ptr dst, mstring_ptr src, index_t size ) noexcept
        {
            if ( size > dst.size() - dst.offset() )
                fault( "copying to a smaller string" );

            if ( size == index_t::lift( uint64_t( 0 ) ) )
                return dst;

            auto dseg = dst->segment_at_current_offset();
            auto sseg = src->segment_at_current_offset();

            bounds_t bounds;
            values_t values;

            std::copy( dst.bounds().begin(), dseg.begin(), std::back_inserter( bounds ) );
            std::copy( dst.values().begin(), dseg.val_it(), std::back_inserter( values ) );

            if ( dseg.from() == dst.offset() ) {
                if ( dseg.begin() != dst.bounds().begin() ) {
                    if ( *std::prev( dseg.val_it() ) != sseg.value() )
                        bounds.push_back( dseg.from() );
                } else {
                    bounds.push_back( dseg.from() );
                }
            } else {
                bounds.push_back( dseg.from() );
                if ( dseg.value() != sseg.value() ) {
                    values.push_back( dseg.value() );
                    bounds.push_back( dst.offset() );
                }
            }

            while ( ( sseg.from() - src.offset() ) < size ) {
                bounds.push_back( dst.offset() + ( sseg.to() - src.offset() ) );
                values.push_back( sseg.value() );
                ++sseg;
            }

            if ( dst.size() > dst.offset() + size ) {
                auto seg = dst->segment_at_index( dst.offset() + size );
                if ( dseg.value() == values.back() ) {
                    //we will take the value from dst suffix
                    bounds.pop_back();
                    values.pop_back();
                }

                std::copy( dseg.end(), dst.bounds().end(), std::back_inserter( bounds ) );
                std::copy( dseg.val_it(), dst.values().end(), std::back_inserter( values ) );
            }

            dst.data()->bounds = bounds;
            dst.data()->values = values;
            return dst;
        }

        _LART_INLINE
        size_t concrete_offset() const noexcept
        {
            return static_cast< index_t >( _offset ).template lower< size_t >();
        }

        _LART_INLINE
        size_t concrete_size() const noexcept
        {
            return static_cast< index_t >( size() ).template lower< size_t >();
        }

        // returns first nonepmty segment containing '\0' after offset
        _LART_INLINE
        segment_t terminal_segment() noexcept
        {
            auto seg = segment_at_current_offset();
            auto zero = character_t::lift( '\0' );
            while ( seg.begin() != bounds().end() && seg.value() != zero && !seg.empty() )
                ++seg;
            if ( seg.begin() == bounds().end() )
                out_of_bounds_fault();
            return seg;
        }

        index_t terminator() noexcept
        {
            // begining of the segment of the first occurence of '\0' after offset
            return terminal_segment().from();
        }

        _LART_INLINE
        friend void trace( mstring_t &mstr ) noexcept
        {
           __dios_trace_f( "mstring offset %lu size %lu:", mstr.concrete_offset(), mstr.concrete_size() );
           auto seg = mstr.segment_at_current_offset();
           while ( seg.end() != mstr.bounds().end() ) {
               trace( seg );
               ++seg;
           }
        }

        /* detail */

        _LART_INLINE
        void drop( index_t size ) noexcept
        {
            auto to_drop = range( offset, size + offset );

            if ( !to_drop.single_segment() ) {
                if ( *to_drop.bounds.from != offset ) {
                    // keep prefix
                    ++to_drop.bounds.from;
                    ++to_drop.values.from;
                }
                if ( *to_drop.bounds.to != size + offset ) {
                    // keep suffix
                    --to_drop.bounds.to;
                    --to_drop.values.to;
                }
            }

            bounds().erase( to_drop.bounds.from, to_drop.bounds.to );
            values().erase( to_drop.values.from, to_drop.values.to );
        }

        _LART_INLINE
        void insert( mstring_ptr src, index_t size ) noexcept
        {
            auto from_bounds = bounds().begin();
            auto from_values = values().begin();
            while ( from_bounds != bounds().end() && *from_bounds < offset ) {
                ++from_bounds; ++from_values;
            }

            auto in = src->range( src.offset(), size );
            // insert bounds from src
            bounds_t tmp_bounds;
            std::move( from_bounds, bounds().end(), std::back_inserter( tmp_bounds ) );
            bounds().erase( from_bounds, bounds().end() );

            for ( const auto& bound : in.bounds )
                bounds().push_back( offset + ( bound - src.offset() ) );
            bounds().push_back( offset + size );
            bounds().append( tmp_bounds.size(), tmp_bounds.begin(), tmp_bounds.end() );

            // insert values from src
            values_t tmp_values;
            std::move( from_values, values().end(), std::back_inserter( tmp_values ) );
            values().erase( from_values, values().end() );

            for ( const auto& value : in.values )
                values().push_back( value );
            values().append( tmp_values.size(), tmp_values.begin(), tmp_values.end() );
        }

        _LART_INLINE
        segment_t segment_at_index( index_t idx ) noexcept
        {
            if ( static_cast< bool >( idx >= size() ) )
                out_of_bounds_fault();

            auto it = bounds().begin();
            for ( auto it = bounds().begin(); std::next( it ) != bounds().end(); ++it )
            {
                if ( idx >= *it && idx < *std::next( it ) ) {
                    auto nth = std::distance( bounds().begin(), it );
                    return segment_t{ it, std::next( values().begin(), nth ) };
                }
            }

            UNREACHABLE( "MSTRING ERROR: index out of bounds" );
        }

        _LART_INLINE
        segment_t segment_at_current_offset() noexcept
        {
            return segment_at_index( _offset );
        }
    };

} // namespace __dios::rst::abstract
