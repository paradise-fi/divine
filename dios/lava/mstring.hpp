// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2020 Petr Ročkai <code@fixp.eu>
 * (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#pragma once

#include "base.hpp"
#include "scalar.hpp"
#include <brick-ptr>

namespace __lava
{
    struct base : brq::refcount_base< uint16_t, false /* atomic */ >
    {
        static void* operator new( size_t s ) { return __vm_obj_make( s, _VM_PT_Heap ); }
        static void operator delete( void *p ) { return __vm_obj_free( p ); }
    };

    template< typename index_t, typename char_t >
    struct mstring_data
    {
        using values_t = __dios::Array< char_t, _VM_PT_Weak >;
        using bounds_t = __dios::Array< index_t, _VM_PT_Weak >;

        struct content_t : base
        {
            values_t values;
            bounds_t bounds;
            index_t size;

            explicit content_t( index_t sz ) : size( sz ) {}
        };

        using content_ptr = brq::refcount_ptr< content_t >;

        index_t offset;
        content_ptr content;

        mstring_data( content_ptr c, index_t off ) : offset( off ), content( c ) {}
    };

    template< typename index_dom_, typename char_dom >
    struct mstring : tagged_storage< mstring_data< scalar< index_dom_ >, scalar< char_dom > > >,
                     domain_mixin< mstring< index_dom_, char_dom > >
    {
        using index_dom = index_dom_;
        using scalar_dom = char_dom;

        using index_t = scalar< index_dom >;
        using char_t  = scalar< char_dom >;
        using data_t  = mstring_data< index_t, char_t >;
        using base    = tagged_storage< data_t >;
        using mref    = const mstring &;

        using content_ptr = typename data_t::content_ptr;
        using content_t   = typename data_t::content_t;
        using values_t    = typename data_t::values_t;
        using bounds_t    = typename data_t::bounds_t;

        using values_iterator = typename values_t::iterator;
        using bounds_iterator = typename bounds_t::iterator;

        using base::get;

        static void fault( const char * msg, _VM_Fault fault_v = _VM_F_Assert )
        {
            __dios_fault( fault_v, msg );
        }

        static void out_of_bounds_fault()
        {
            fault( "Access out of bounds.", _VM_F_Memory );
        }

        auto        data()    const { return get().content; }
        auto       &content() const { return *data(); }
        auto       &bounds()        { return content().bounds; }
        const auto &bounds()  const { return content().bounds; }
        auto       &values()        { return content().values; }
        const auto &values()  const { return content().values; }

        const index_t &size()   const { return content().size; }
        const index_t &offset() const { return get().offset; }

        const index_t &checked_offset() const
        {
            if ( offset() >= size() )
                out_of_bounds_fault();
            return offset();
        }

        void push_bound( index_t bound ) { bounds().push_back( bound ); }
        void push_bound( uint64_t b ) { push_bound( index_t::lift( b ) ); }
        void push_char( char_t ch ) { values().push_back( ch ); }
        void push_char( char ch ) { push_char( char_t::lift( ch ) ); }

        mstring( content_ptr content, index_t offset )
            : base( data_t{ content, offset } )
        {}

        mstring( index_t size )
            : mstring( brq::make_refcount< content_t >( size ), index_t::lift( 0ul ) )
        {}

        mstring( void *v, __dios::construct_shared_t s ) : base( v, s ) {}

        template< typename T >
        static mstring any()
        {
            return { index_t::template any< size_t >() };
        }

        static mstring assume( mref s, mref, bool ) { return s; }
        static tristate to_tristate( mref ) { return tristate::yes; }

        template< typename T >
        static std::enable_if_t< std::is_integral_v< T >, mstring > lift( T t )
        {
            return { nullptr, index_t::lift( t ) };
        }

        static mstring lift( float ) { NOT_IMPLEMENTED(); }
        static mstring lift( const index_dom &idx ) { return { nullptr, index_t( idx ) }; }

        static mstring lift( array_ref arr )
        {
            auto str = static_cast< char * >( arr.base );

            mstring mstr( index_t::lift( arr.size ) );
            mstr.push_bound( uint64_t( 0 ) );

            if ( arr.size == 0 )
                return mstr;

            char prev = str[ 0 ];

            for ( uint64_t i = 1; i < arr.size; ++i )
            {
                if ( prev != str[ i ] )
                {
                    mstr.push_char( prev );
                    mstr.push_bound( i );
                    prev = str[ i ];
                }
            }

            mstr.push_char( prev );
            mstr.push_bound( arr.size );

            return mstr;
        }

        static mstring op_thaw( mref str, uint8_t bw )
        {
            return str;
        }

        /* implementation of abstraction interface */

        struct segment_t
        {
            bounds_iterator _from;
            values_iterator _value;

            bounds_iterator begin() { return _from; }
            bounds_iterator begin() const { return _from; }
            bounds_iterator end() { return std::next( begin(), 2 ); }
            bounds_iterator end() const { return std::next( begin(), 2 ); }

            index_t &from()  const { return *begin(); }
            index_t &to()    const { return *std::next( begin() ); }
            char_t  &value() const { return *_value; }

            void set_char( char_t ch ) { *_value = ch; }

            char_t &operator*() & { return *_value; }
            char_t &operator*() && = delete;
            char_t *operator->() const { return _value; }

            friend bool operator==( const segment_t &lhs, const segment_t &rhs )
            {
                return std::tie( lhs._from, lhs._value ) == std::tie( rhs._from, rhs._value );
            }

            friend bool operator!=( const segment_t &lhs, const segment_t &rhs )
            {
                return !(lhs == rhs );
            }

            inline segment_t& operator+=( const int& rhs )
            {
                _from += rhs;
                _value += rhs;
                return *this;
            }

            inline segment_t& operator-=( const int& rhs )
            {
                _from -= rhs;
                _value -= rhs;
                return *this;
            }

            segment_t& operator++()
            {
                ++_from;
                ++_value;
                return *this;
            }

            segment_t operator++(int)
            {
                segment_t tmp(*this);
                operator++();
                return tmp;
            }

            segment_t& operator--()
            {
                --_from; --_value;
                return *this;
            }

            segment_t operator--(int)
            {
                segment_t tmp(*this);
                operator++();
                return tmp;
            }

            bool empty() const { return static_cast< bool >( from() == to() ); }
            bool has_value( char ch ) const { return has_value( char_t::lift( ch ) ); }

            bool has_value( char_t ch ) const
            {
                if ( empty() )
                    return false;
                return static_cast< bool >( value() == ch );
            }

            bool singleton() const
            {
                return static_cast< bool >( from() + index_t::lift( uint64_t( 1 ) ) == to() );
            }
        };

        struct segments_range
        {
            template< typename iterator_t >
            struct range_t
            {
                iterator_t _begin;
                iterator_t _end;

                auto begin() { return _begin; }
                auto begin() const { return _begin; }
                auto end() { return _end; }
                auto end() const { return _end; }
            };

            segment_t begin() { return { bounds.begin(), values.begin() }; }
            segment_t end() { return { bounds.end(), values.end() }; }

            segment_t next_segment( segment_t seg )
            {
                do { ++seg; } while ( seg.begin() != bounds.end() && seg.empty() );
                return seg;
            }

            segment_t terminal() const
            {
                return { std::prev( bounds.end() ), std::prev( values.end() ) };
            }

            range_t< bounds_iterator > bounds;
            range_t< values_iterator > values;
        };

        segments_range range( segment_t f, segment_t t ) const
        {
            return { { f.begin(), std::next( t.begin() ) }, { f._value, std::next( t._value ) } };
        }

        segments_range range( index_t from, index_t to ) const
        {
            auto f = segment_at_index( from );
            auto t = segment_at_index( to );
            return range( f, t );
        }

        segments_range interest() const
        {
            return range( segment_at_current_offset(), terminal_segment() );
        }

        static char_dom op_load( mref array, bitwidth_t bw )
        {
            if ( bw != 8 )
                fault( "unexpected load bitwidth" );

            return array.segment_at_current_offset().value();
        }

        void store( const char_t &ch, const index_t &idx )
        {
            auto one = index_t::lift( uint64_t( 1 ) );
            auto seg = segment_at_index( idx );

            if ( seg.value() == ch )
                ; // do nothing
            else if ( seg.singleton() )
                // rewrite single character segment
                seg.set_char( ch );
            else if ( seg.from() == idx )
            {
                // rewrite first character of segment
                if ( seg.begin() != bounds().begin() )
                {
                    auto prev = seg; --prev;
                    if ( prev.value() == ch )
                    {
                        // merge with left neighbour
                        seg.from() = seg.from() + one;
                        return;
                    }
                }

                bounds().insert( std::next( seg.begin() ), idx + one );
                values().insert( &seg.value(), ch );
            }
            else if ( seg.to() - one == idx )
            {
                // rewrite last character of segment
                if ( seg.end() != bounds().end() )
                {
                    auto next = seg; ++next;
                    if ( next.value() == ch )
                    {
                        // merge with left neighbour
                        seg.to() = seg.to() - one;
                        return;
                    }
                }
                bounds().insert( std::next( seg.begin() ), idx );
                values().insert( std::next( &seg.value() ), ch );
            }
            else
            {
                // rewrite segment in the middle (split segment)
                auto seg_v = seg.value();
                auto vit = values().insert( &seg.value(), seg_v );
                values().insert( std::next( vit ), ch );
                auto bit = bounds().insert( std::next( seg.begin() ), idx );
                bounds().insert( std::next( bit ), idx + one );
            }
        }

        template< typename char_in >
        static mstring op_store( mref array, const char_in &value, bitwidth_t bw )
        {
            if constexpr ( std::is_same_v< char_in, char_dom > )
            {
                if ( bw != 8 )
                    fault( "unexpected store bitwidth" );

                auto &str = const_cast< mstring & >( array );
                str.store( char_t( value ).zfit( 8 ), array.checked_offset() );
                return { nullptr, index_t::lift( 0ul ) };
            }
            else
            {
                __dios_trace_f( "boom %s", __PRETTY_FUNCTION__ );
                __builtin_trap();
            }
        }

        template< typename index_in >
        static mstring op_gep( mref array, const index_in &idx, size_t elem_size )
        {
            ASSERT_EQ( elem_size, 8 ); /* FIXME */
            if constexpr ( std::is_same_v< index_in, index_dom > )
                return { array.data(), array.offset() + index_t( idx ) };
            else
                __builtin_trap();
        }

        static index_dom op_sub( mref lhs, mref rhs )
        {
            if ( &lhs.content() != &rhs.content() )
                fault( "comparing pointers of different objects" );
            return lhs.offset() - rhs.offset();
        }

        static index_dom op_eq( mref lhs, mref rhs )
        {
            if ( !lhs.data() || !rhs.data() || ( lhs.data() != rhs.data() ) )
                return index_t::lift( false );
            else
                return lhs.offset() == rhs.offset();
        }

        static index_dom fn_strlen( mref str )
        {
            return ( str.terminator() - str.offset() ).zfit( 64 );
        }

        static scalar_dom fn_strcmp( mref lhs, mref rhs )
        {
            auto lin = lhs.interest();
            auto rin = rhs.interest();

            auto lseg = lin.begin();
            if ( lseg.empty() )
                lseg = lin.next_segment( lseg );
            auto rseg = rin.begin();
            if ( rseg.empty() )
                rseg = rin.next_segment( rseg );

            while ( lseg != lin.terminal() && rseg != rin.terminal() )
            {
                if ( lseg.value() != rseg.value() )
                    return ( lseg.value() - rseg.value() ).zfit( 32 );
                else
                {
                    index_t left = lseg.to() - lhs.offset();
                    index_t right = rseg.to() - rhs.offset();
                    if ( left > right )
                        return ( lseg.value() - rin.next_segment( rseg ).value() ).zfit( 32 );
                    else if ( left < right )
                        return ( lin.next_segment( lseg ).value() - rseg.value() ).zfit( 32 );
                }

                lseg = lin.next_segment( lseg );
                rseg = rin.next_segment( rseg );
            }

            return ( lseg.value() - rseg.value() ).zfit( 32 );
        }

        static mstring fn_strcat( mref dst, mref src ) /* TODO optimize */
        {
            auto off = dst.offset();
            auto size = src.terminator() - src.offset();
            const_cast< mstring & >( dst ).get().offset = dst.terminator();
            auto ret = fn_memcpy( dst, src, size );
            ret.get().offset = off;
            return ret;
        }

        static mstring fn_strcpy( mref dst, mref src ) /* TODO optimize */
        {
            return fn_memcpy( dst, src, index_t( fn_strlen( src ) ) + index_t::lift( size_t( 1 ) ) );
        }

        static mstring fn_strchr( mref str, const char_dom &ch_ )
        {
            auto ch = char_t( ch_ ).zfit( 8 );
            auto seg = str.segment_at_current_offset();
            int iter = 0;

            while ( !str.is_beyond_last_segment( seg ) )
            {
                if ( seg.has_value( ch ) )
                {
                    auto off = seg.from();
                    if ( seg.begin() == str.bounds().begin() )
                        return str;
                    return { str.data(), off };
                }
                ++seg;
            }

            return { nullptr, index_t::lift( 0ul ) };
        }

        template< typename char_in >
        static mstring fn_strchr( mref str, const char_in & )
        {
            __builtin_trap();
        }

        static mstring fn_memcpy( mref dst, mref src, index_t size )
        {
            if ( size > dst.size() - dst.offset() )
                fault( "copying to a smaller string" );

            if ( size == index_t::lift( uint64_t( 0 ) ) )
                return dst;

            auto dseg = dst.segment_at_current_offset();
            auto sseg = src.segment_at_current_offset();

            const auto *dseg_begin = dseg.begin();
            const auto *dseg_end = &dseg.value();

            bounds_t bounds;
            values_t values;

            std::copy( dst.bounds().begin(), dseg_begin, std::back_inserter( bounds ) );
            std::copy( dst.values().begin(), dseg_end, std::back_inserter( values ) );

            if ( dseg.from() == dst.offset() )
                bounds.push_back( dseg.from() );
            else
            {
                bounds.push_back( dseg.from() );
                if ( dseg.value() != sseg.value() )
                {
                    values.push_back( dseg.value() );
                    bounds.push_back( dst.offset() );
                }
            }

            if ( values.size() > 0 && static_cast< bool >( values.back() == sseg.value() ) )
            {
                values.pop_back();
                bounds.pop_back();
            }

            while ( ( sseg.from() - src.offset() ) < size )
            {
                bounds.push_back( dst.offset() + ( sseg.to() - src.offset() ) );
                values.push_back( sseg.value() );
                ++sseg;
            }

            if ( dst.size() > dst.offset() + size )
            {
                auto seg = dst.segment_at_index( dst.offset() + size );
                if ( dseg.value() == values.back() )
                {
                    // we will take the value from dst suffix
                    bounds.pop_back();
                    values.pop_back();
                }

                const auto *dseg_begin = dseg.begin();
                const auto *dseg_end = &dseg.value();

                std::copy( std::next( dseg_begin ), dst.bounds().end(), std::back_inserter( bounds ) );
                std::copy( dseg_end, dst.values().end(), std::back_inserter( values ) );
            }

            dst.content().bounds = std::move( bounds );
            dst.content().values = std::move( values );
            return dst;
        }

        // returns first nonepmty segment containing '\0' after offset
        segment_t terminal_segment() const
        {
            auto seg = segment_at_current_offset();
            while ( !is_beyond_last_segment( seg ) && !seg.has_value( '\0' ) ) {
                ++seg;
            }
            if ( is_beyond_last_segment( seg ) )
                out_of_bounds_fault();
            return seg;
        }

        const index_t &terminator() const
        {
            // begining of the segment of the first occurence of '\0' after offset
            return terminal_segment().from();
        }

        /* detail */

        bool is_beyond_last_segment( const segment_t& seg ) const
        {
            return seg.begin() == std::prev( bounds().end() );
        }

        segment_t segment_at_index( index_t idx ) const
        {
            if ( idx >= size() )
                out_of_bounds_fault();

            auto *b_begin = const_cast< index_t * >( bounds().begin() );
            auto *v_begin = const_cast< char_t * >( values().begin() );

            for ( auto it = b_begin; std::next( it ) != bounds().end(); ++it )
            {
                if ( idx >= *it && idx < *std::next( it ) )
                {
                    auto nth = std::distance( b_begin, it );
                    return segment_t{ it, std::next( v_begin, nth ) };
                }
            }

            __builtin_trap();
        }

        segment_t segment_at_current_offset() const
        {
            return segment_at_index( offset() );
        }
    };

}
