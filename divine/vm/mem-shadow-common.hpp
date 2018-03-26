// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2018 Adam Matoušek <xmatous3@fi.muni.cz>
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

#include <brick-types>

#include <divine/vm/value.hpp>

namespace divine {
namespace vm::mem {

namespace bitlevel = brick::bitlevel;

template< typename InternalPtr >
struct InternalLoc : public brick::types::Ord
{
    InternalPtr object;
    int offset;
    InternalLoc( InternalPtr o, int off = 0 )
        : object( o ), offset( off )
    {}
    InternalLoc operator-( int i ) const { InternalLoc r = *this; r.offset -= i; return r; }
    InternalLoc operator+( int i ) const { InternalLoc r = *this; r.offset += i; return r; }
    bool operator==( const InternalLoc & o) const
    {
        return object == o.object && offset == o.offset;
    }
    bool operator<=(const InternalLoc & o) const
    {
        return object < o.object || (object == o.object && offset <= o.offset);
    }
    bool operator<(const InternalLoc & o) const
    {
        return object < o.object || (object == o.object && offset < o.offset);
    }
};

template< unsigned BitsPerSet, typename Pool >
struct BitsetContainer
{
    static_assert( BitsPerSet && ( ( BitsPerSet & ( BitsPerSet - 1 ) ) == 0 ),
                   "BitsPerSet must be a power of two" );

    using Type = bitlevel::bitvec< BitsPerSet >;
    using Ptr = typename Pool::Pointer;
    using Loc = InternalLoc< Ptr >;

    struct proxy
    {
        Type *_base; int _pos;

        proxy( Type *b, int p ) : _base( b ), _pos( p ) {}
        Type & word() { return *( _base + ( _pos * BitsPerSet ) / ( 8 * sizeof( Type ) ) ); }
        Type word() const { return *( _base + ( _pos * BitsPerSet ) / ( 8 * sizeof( Type ) ) ); }
        unsigned shift() const {
            constexpr unsigned SetsInType = ( 8 * sizeof( Type ) ) / BitsPerSet;
            return BitsPerSet * ( SetsInType - 1 - _pos % SetsInType );
        }
        constexpr Type mask() const { return bitlevel::ones< Type >( BitsPerSet ); }
        proxy &operator=( const proxy &o ) { return *this = Type( o ); }
        proxy &operator=( Type b ) { set( b ); return *this; }
        Type get() const { return ( word() >> shift() ) & mask(); }
        void set( Type t )
        {
            word() &= ~( mask() << shift() );
            word() |= ( t & mask() ) << shift();
        }
        operator Type() const { return get(); }
        bool operator==( const proxy &o ) const { return get() == o.get(); }
        bool operator!=( const proxy &o ) const { return get() != o.get(); }
        proxy *operator->() { return this; }
    };
    struct iterator : std::iterator< std::forward_iterator_tag, proxy >
    {
        Type *_base; int _pos;

        iterator( Type *b, int p ) : _base( b ), _pos( p ) {}
        proxy operator*() const { return proxy( _base, _pos ); }
        proxy operator->() const { return proxy( _base, _pos ); }
        iterator &operator++() { ++_pos; return *this; }
        iterator operator++( int ) { auto i = *this; ++_pos; return i; }
        iterator &operator+=( int off ) { _pos += off; return *this; }
        iterator operator+( int off ) const { auto r = *this; r._pos += off; return r; }
        bool operator!=( iterator o ) const { return _base != o._base || _pos != o._pos; }
        bool operator==( iterator o ) const { return _base == o._base && _pos == o._pos; }
        bool operator<( iterator o ) const { return _pos < o._pos; }
    };

    Type *_base;
    int _from, _to;

    BitsetContainer( Pool &p, Ptr b, int f, int t )
        : _base( p.template machinePointer< Type >( b ) ), _from( f ), _to( t ) {}
    iterator begin() { return iterator( _base, _from ); }
    iterator end() { return iterator( _base, _to ); }

    proxy operator[]( int i )
    {
        ASSERT_LT( _from + i, _to );
        return proxy( _base, _from + i );
    }
};

template< typename ExceptionType, typename Internal >
class ExceptionMap
{
public:
    using Loc = InternalLoc< Internal >;
    using ExcMap = std::map< Loc, ExceptionType >;
    using Lock = std::lock_guard< std::mutex >;

    ExceptionMap &operator=( const ExceptionType & o ) = delete;

    ExceptionType at( Internal obj, int wpos )
    {
        ASSERT_EQ( wpos % 4, 0 );

        Lock lk( _mtx );

        auto it = _exceptions.find( Loc( obj, wpos ) );
        ASSERT( it != _exceptions.end() );
        ASSERT( it->second.valid() );
        return it->second;
    }

    void set( Internal obj, int wpos, const ExceptionType &exc )
    {
        ASSERT_EQ( wpos % 4, 0 );

        Lock lk( _mtx );
        _exceptions[ Loc( obj, wpos ) ] = exc;
    }

    void invalidate( Internal obj, int wpos )
    {
        ASSERT_EQ( wpos % 4, 0 );

        Lock lk( _mtx );

        auto it = _exceptions.find( Loc( obj, wpos ) );

        ASSERT( it != _exceptions.end() );
        ASSERT( it->second.valid() );

        it->second.invalidate();
    }

    void free( Internal obj )
    {
        Lock lk( _mtx );

        auto lb = _exceptions.lower_bound( Loc( obj, 0 ) );
        auto ub = _exceptions.upper_bound( Loc( obj, (1 << _VM_PB_Off) - 1 ) );
        while (lb != ub)
        {
            lb->second.invalidate();
            ++lb;
        }
    }

    bool empty()
    {
        Lock lk( _mtx );
        return std::all_of( _exceptions.begin(), _exceptions.end(),
                            []( const auto & e ) { return ! e.second.valid(); } );
    }

protected:
    ExcMap _exceptions;
    mutable std::mutex _mtx;
};

// No-op shadow layer
template< typename _Internal, typename _Expanded >
struct ShadowBottom {
    using Internal = _Internal;
    using Expanded = _Expanded;
    using Loc = InternalLoc< Internal >;

    template< typename V >
    void write( Loc, V, Expanded * ) {}
    template< typename V >
    void read( Loc, V &, Expanded * ) {}
    template< typename FromSh >
    void copy_word( FromSh &, typename FromSh::Loc, Expanded, Loc, Expanded ) {}
    template< typename FromSh, typename FromHeapReader >
    void copy_init_src( FromSh &, typename FromSh::Internal, int off, const Expanded &,
                        FromHeapReader )
    {
        ASSERT_EQ( off % 4, 0 );
    }
    template< typename ToHeapReader >
    void copy_init_dst( Internal, int off, const Expanded &, ToHeapReader )
    {
        ASSERT_EQ( off % 4, 0 );
    }

    template< typename FromSh, typename FromHeapReader, typename ToHeapReader >
    void copy_byte( FromSh &, typename FromSh::Loc, const Expanded &, FromHeapReader,
                    Loc, Expanded &, ToHeapReader ) {}
    void copy_done( Internal, int off, Expanded & )
    {
        ASSERT_EQ( off % 4, 0 );
    }
    template< typename OtherSh >
    int compare_word( OtherSh &, typename OtherSh::Loc a, Expanded, Loc b, Expanded )
    {
            ASSERT_EQ( a.offset % 4, 0 );
            ASSERT_EQ( b.offset % 4, 0 );
            return 0;
    }
    template< typename OtherSh >
    int compare_byte( OtherSh &, typename OtherSh::Loc a, Expanded, Loc b, Expanded )
    {
        ASSERT_EQ( a.offset % 4, b.offset % 4 );
        return 0;
    }

    void make( Internal, int ) {}
    void free( Internal ) {}
};

}

namespace t_vm {

struct BitsetContainer {
    using Pool = brick::mem::Pool<>;
    Pool::Pointer obj;
    Pool pool;
    BitsetContainer() { obj = pool.allocate( 100 ); }

    TEST( _8bit )
    {
        using UC8 = vm::mem::BitsetContainer< 8, Pool >;
        UC8 uc( pool, obj, 0, 100 );
        static_assert( sizeof( UC8::Type ) == 1, "" );

        ASSERT_EQ( uc.begin()->mask(), 0xFF );
        for ( int i = 0; i < 8; ++i )
            ASSERT_EQ( ( uc.begin() + i )->shift(), 0 );

        auto *b = &uc.begin()->word();
        for ( int i = 1; i < 8; ++i )
            ASSERT_EQ( &uc[ i ].word(), b + i );

        int a = 0;
        for ( auto i = uc.begin(); i < uc.end(); ++i )
            i->set( a++ );

        ASSERT_EQ( a, 100 );

        a = 0;
        for ( auto i = uc.begin(); i < uc.end(); ++i )
            ASSERT_EQ( *i, a++ );
    }

    TEST( _4bit )
    {
        using UC4 = vm::mem::BitsetContainer< 4, Pool >;
        UC4 uc( pool, obj, 0, 200 );
        static_assert( sizeof( UC4::Type ) == 1, "" );

        ASSERT_EQ( uc.begin()->mask(), 0x0F );
        for ( int i = 0; i < 8; ++i )
            ASSERT_EQ( ( uc.begin() + i )->shift(), ( ( i + 1 ) % 2 ) * 4 );

        auto *b = &uc.begin()->word();
        for ( int i = 1; i < 16; ++i )
            ASSERT_EQ( &uc[ i ].word(), b + i / 2 );

        int a = 0;
        for ( auto pxy : uc )
            pxy = a++;

        ASSERT_EQ( a, 200 );

        a = 0;
        for ( auto pxy : uc )
            ASSERT_EQ( pxy, a++ % 16 );
    }

    TEST( _2bit )
    {
        using UC2 = vm::mem::BitsetContainer< 2, Pool >;
        UC2 uc( pool, obj, 0, 400 );
        static_assert( sizeof( UC2::Type ) == 1, "" );

        ASSERT_EQ( uc.begin()->mask(), 0x03 );
        for ( int i = 0; i < 8; ++i )
            ASSERT_EQ( ( uc.begin() + i )->shift(), 6 - 2 * ( i % 4 ) );

        auto *b = &uc.begin()->word();
        for ( int i = 1; i < 16; ++i )
            ASSERT_EQ( &uc[ i ].word(), b + i / 4 );

        int a = 0;
        for ( auto i = uc.begin(); i < uc.end(); ++i )
            i->set( a++ );

        ASSERT_EQ( a, 400 );

        a = 0;
        for ( auto i = uc.begin(); i < uc.end(); ++i )
            ASSERT_EQ( *i, a++ % 4 );
    }

    TEST( _1bit )
    {
        using UC1 = vm::mem::BitsetContainer< 1, Pool >;
        UC1 uc( pool, obj, 0, 800 );
        static_assert( sizeof( UC1::Type ) == 1, "" );

        ASSERT_EQ( uc.begin()->mask(), 0x01 );
        for ( int i = 0; i < 8; ++i )
            ASSERT_EQ( ( uc.begin() + i )->shift(), 7 - i );

        auto *b = &uc.begin()->word();
        for ( int i = 1; i < 16; ++i )
            ASSERT_EQ( &uc[ i ].word(), b + i / 8 );

        int a = 0;
        for ( auto i = uc.begin(); i < uc.end(); ++i )
            i->set( a++ % 3 == 1 );

        ASSERT_EQ( a, 800 );

        a = 0;
        for ( auto i = uc.begin(); i < uc.end(); ++i )
            ASSERT_EQ( *i, a++ % 3 == 1 );
    }


    TEST( _16bit )
    {
        using UC16 = vm::mem::BitsetContainer< 16, Pool >;
        UC16 uc( pool, obj, 0, 50 );
        static_assert( sizeof( UC16::Type ) == 2, "" );

        ASSERT_EQ( uc.begin()->mask(), 0xFFFF );
        for ( int i = 0; i < 8; ++i )
            ASSERT_EQ( ( uc.begin() + i )->shift(), 0 );

        auto *b = &uc.begin()->word();
        for ( int i = 1; i < 4; ++i )
            ASSERT_EQ( &uc[ i ].word(), b + i );

        int a = 0x0EFF;
        for ( auto i = uc.begin(); i < uc.end(); ++i )
            i->set( a++ );

        ASSERT_EQ( a, 0x0EFF + 50 );

        a = 0x0EFF;
        for ( auto i = uc.begin(); i < uc.end(); ++i )
            ASSERT_EQ( *i, a++ );
    }

    TEST( copy )
    {
        using UC8 = vm::mem::BitsetContainer< 8, Pool >;
        UC8 uc( pool, obj, 0, 100 );

        auto a = uc.begin();
        auto b = uc.begin() + 50;
        for ( int i = 0; i < 50; ++i )
            uc[ i ] = i;

        for ( int i = 0; i < 50; ++i )
            *b++ = *a++;

        a = uc.begin();
        b = uc.begin() + 50;
        for ( int i = 0; i < 50; ++i )
        {
            ASSERT_EQ( uc[ i ], i );
            ASSERT_EQ( *a, i );
            ASSERT_EQ( *a++, *b++ );
        }
    }
};

}

}
