// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * Supplementary data structures
 */

/*
 * (c) 2014-2015 Vladimír Štill <xstill@fi.muni.cz>
 */

/* Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE. */

#include <map>
#include <vector>
#include <utility>
#include <type_traits>
#include <stdexcept>
#include <initializer_list>
#include <array>
#include <iterator>
#include <memory>

#include <brick-assert.h>

#ifndef BRICK_DATA_H
#define BRICK_DATA_H

// gcc is missing is_trivially_default_constructible (as of 4.9!)
// furthermore 4.7 is missing is_trivially_destructible but 4.8 has it and
// is missing old trait has_trivial_destructor
// https://stackoverflow.com/questions/12702103/writing-code-that-works-when-has-trivial-destructor-is-defined-instead-of-is
// BUG: clang does not compile code in ifdef __GLIBCXX__ even if it is
// compiled with libstdc++, this is no problem in DIVINE as we support either
// clang with libc++ or gcc with libstdc++ (clang with libstdc++ is not able
// to compile brick-hashset too)
#ifdef __GLIBCXX__
namespace std {
    template< typename > struct has_trivial_default_constructor;
    template< typename > struct is_trivially_default_constructible;
    template< typename > struct has_trivial_destructor;
    template< typename > struct is_trivially_destructible;
}
#endif

namespace brick {
namespace data {

#ifdef __GLIBCXX__
template< typename T >
class have_cxx11_ctor_trait {

    template< typename T2, bool = std::is_trivially_default_constructible< T2 >::type::value >
    static std::true_type test(int);

    template< typename T2, bool = std::has_trivial_default_constructor< T2 >::type::value >
    static std::false_type test(...);

    public:
    typedef decltype( test<T>(0) ) type;
};

template< typename T >
constexpr bool __triviallyDefaultConstructible() {
    return std::conditional< have_cxx11_ctor_trait< T >::type::value,
                std::is_trivially_default_constructible< T >,
                std::has_trivial_default_constructor< T > >::type::value;
}

template< typename T >
class have_cxx11_dtor_trait {

    template< typename T2, bool = std::is_trivially_destructible< T2 >::type::value >
    static std::true_type test(int);

    template< typename T2, bool = std::has_trivial_destructor< T2 >::type::value >
    static std::false_type test(...);

    public:
    typedef decltype( test<T>(0) ) type;
};

template< typename T >
constexpr bool __triviallyDestructible() {
    return std::conditional< have_cxx11_dtor_trait< T >::type::value,
                std::is_trivially_destructible< T >,
                std::has_trivial_destructor< T > >::type::value;
}
#else
template< typename T >
constexpr bool __triviallyDefaultConstructible() {
    return std::is_trivially_default_constructible< T >::value;
}

template< typename T >
constexpr bool __triviallyDestructible() {
    return std::is_trivially_destructible< T >::value;
}
#endif

template< typename K, typename V >
struct RMap : std::map< K, V > {

    template< typename... Args >
    RMap( Args &&...args ) : std::map< K, V >( std::forward< Args >( args )... ) { }

    const V &operator[]( const K &key ) const {
        return this->at( key );
    }
};

template< typename A, typename B > struct Bimap;

template< typename A, typename B >
struct _BimapIx {

    B operator[]( const A &key ) const { return self().left()[ key ]; }
    A operator[]( const B &key ) const { return self().right()[ key ]; }

  private:
    const Bimap< A, B > &self() const {
        return *static_cast< const Bimap< A, B > * >( this );
    }
};

template< typename A >
struct _BimapIx< A, A > { };

template< typename A, typename B >
struct Bimap : _BimapIx< A, B > {
    using value_type = std::pair< const A, const B >;

    static_assert( std::is_same< value_type,
            typename std::map< A, const B >::value_type >::value,
            "value_type" );

    Bimap() = default;

    Bimap( std::initializer_list< value_type > init ) {
        for ( auto &p : init )
            insert( std::move( p ) );
    }

    template< typename Iterator >
    Bimap( Iterator begin, Iterator end ) {
        for ( ; begin != end; ++begin )
            insert( *begin );
    }

    const RMap< A, const B > &left() const { return _left; }
    const RMap< B, const A > &right() const { return _right; }

    bool insert( const value_type &value ) {
        return insert( value.first, value.second );
    }

    bool insert( const A &a, const B &b ) {
        auto l = _left.insert( { a, b } );
        auto r = _right.insert( { b, a } );
        if ( l.second != r.second ) {
            if ( l.second )
                _left.erase( l.first );
            else
                _right.erase( r.first );
            throw std::invalid_argument( "Bimap: Insert would break bijection" );
        }
        return l.second;
    }

  private:
    RMap< A, const B > _left;
    RMap< B, const A > _right;

    static std::pair< const B, const A > swap( const std::pair< const A, const B > &pair ) {
        return { pair.second, pair.first };
    }
    static std::pair< const B, const A > swap( std::pair< const A, const B > &&pair ) {
        return { std::move( pair.second ), std::move( pair.first ) };
    }
};

namespace uninitialized {
    template< typename InputIt, typename ForwardIt >
    ForwardIt move( InputIt first, InputIt last, ForwardIt dst ) {
        using T = typename std::iterator_traits< ForwardIt >::value_type;
        ForwardIt current = dst;
        try {
            for ( ; first != last; ++first, ++current )
                new ( static_cast< void * >( &*current ) ) T( *first );
            return current;
        } catch ( ... ) {
            for ( ; dst != current; ++dst )
                dst->~T();
            throw;
        }
    }

    template< typename T, typename ForwardIt >
    auto construct( ForwardIt, ForwardIt ) ->
        typename std::enable_if< __triviallyDefaultConstructible< T >() >::type
    { }

    template< typename T, typename ForwardIt >
    auto construct( ForwardIt first, ForwardIt last ) ->
        typename std::enable_if< !__triviallyDefaultConstructible< T >() >::type
    {
        ForwardIt current = first;
        try {
            for ( ; current != last; ++current )
                new ( static_cast< void * >( &*current ) ) T();
        } catch ( ... ) {
            for ( ; first != current; ++first )
                first->~T();
            throw;
        }
    }
}

template< typename T, int stackCapacity = (4 > (2 * sizeof( void * )) / sizeof( T ))
                                            ? 4 : (2 * sizeof( void * )) / sizeof( T ) >
struct SmallVector {
    SmallVector() : _size( 0 ), _onstack( true ) { }

    template< typename InputIt >
    SmallVector( InputIt begin, InputIt end ) : SmallVector() {
        _resize( std::distance( begin, end ) );
        std::uninitialized_copy( begin, end, _begin() );
    }

    SmallVector( std::initializer_list< T > init ) : SmallVector() {
        _resize( init.size() );
        uninitialized::move( init.begin(), init.end(), begin() );
    }

    SmallVector( const SmallVector &other ) : SmallVector() {
        _resize( other._size );
        std::uninitialized_copy( other.begin(), other.end(), begin() );
    }

    SmallVector( SmallVector &&other ) : SmallVector() {
        _resize( other._size );
        uninitialized::move( other.begin(), other.end(), begin() );
        other.resize( 0 );
    }

    ~SmallVector() {
        _drop< T >( _begin(), _end() );
        if ( !_onstack )
            delete[] _data.range.begin;
    }

    T *begin() { return _ptrCast( _begin() ); }
    const T *begin() const { return _ptrCast( _begin() ); }
    const T *cbegin() const { return _ptrCast( _begin() ); }
    T &front() { return begin()[ 0 ]; }
    const T &front() const { return begin()[ 0 ]; }

    T *end() { return begin() + _size; }
    const T *end() const { return begin() + _size; }
    const T *cend() const { return begin() + _size; }
    T &back() { return end()[ -1 ]; }
    const T &back() const { return end()[ -1 ]; }

    T *data() { return begin(); }
    const T *data() const { return begin(); }

    long size() const { return _size; }
    long empty() const { return _size == 0; }

    long capacity() const {
        return _onstack ? stackCapacity : _data.range.capacity;
    }

    T &operator[]( long ix ) { return begin()[ ix ]; }
    const T &operator[]( long ix ) const { return begin()[ ix ]; }

    template< typename... Args >
    void emplace_back( Args&&... args ) {
        _resize( _size + 1 );
        new ( end() - 1 ) T( std::forward< Args >( args )... );
    }

    void push_back( const T &value ) { emplace_back( value ); }
    void pop_back() { _resize( _size - 1 ); }

    void resize( long count ) {
        long old = _size;
        _resize( count );
        auto from = begin() + old;
        auto to = end();
        if ( from < to )
            uninitialized::construct< T >( from, to );
    }

    void resize( long count, const T &value ) {
        long old = _size;
        _resize( count );
        auto from = begin() + old;
        auto to = end();
        if ( from < to )
            std::uninitialized_fill( from, to, value );
    }

    void reserve( long newcap ) {
        if ( newcap > capacity() )
            _reserve( newcap );
    }

    void clear() { _resize( 0 ); }
    void clear_and_drop_memory() {
        clear();
        if ( !_onstack ) {
            delete[] _data.range.begin;
            _data.range.begin = nullptr;
            _onstack = true;
        }
    }

  private:
    using Storage = typename std::aligned_storage< sizeof( T ), alignof( T ) >::type;
    static_assert( sizeof( Storage ) == sizeof( T ), "storage size" );

    static T *_ptrCast( Storage *ptr ) {
        union { Storage *st; T *t; };
        st = ptr;
        return t;
    }

    static const T *_ptrCast( const Storage *ptr ) {
        union { const Storage *st; const T *t; };
        st = ptr;
        return t;
    }

    struct Range {
        Storage *begin;
        long capacity;
    };
    union Data {
        Range range;
        Storage data[ stackCapacity ];
    };

    Data _data;
    long _size : sizeof( long ) * 8 - 1;
    bool _onstack : 1;

    Storage *_begin() {
        return _onstack ? _data.data : _data.range.begin;
    }

    const Storage *_begin() const {
        return _onstack ? _data.data : _data.range.begin;
    }

    Storage *_end() { return _begin() + _size; }

    template< typename X >
    auto _drop( Storage *from, Storage *to ) ->
        typename std::enable_if< __triviallyDestructible< X >() >::type
    { }

    template< typename X >
    auto _drop( Storage *from, Storage *to ) ->
        typename std::enable_if< !__triviallyDestructible< X >() >::type
    {
        for ( ; from < to; ++from )
            _ptrCast( from )->~X();
    }

    void _reserve( long count ) {
        if ( count < _size )
            _drop< T >( _begin() + count, _end() );
        else if ( count > capacity() ) {
            long newcap = 1;
            for ( ; newcap < count; newcap <<= 1 ) { }
            std::unique_ptr< Storage[] > nstor( new Storage[ newcap ] );
            uninitialized::move( begin(), end(), _ptrCast( nstor.get() ) );
            _drop< T >( _begin(), _end() );
            if ( !_onstack )
                delete[] _data.range.begin;
            _data.range.begin = nstor.release();
            _data.range.capacity = newcap;
            _onstack = false;
        }
    }

    void _resize( long count ) {
        _reserve( count );
        _size = count;
    }
};

}
}

namespace brick_test {
namespace data {
using namespace brick::data;

#define TC( X ) do { try { X; } catch ( std::out_of_range &ex ) { \
        throw std::runtime_error( "caught: " + std::string( ex.what() ) + \
                " at " __FILE__ ":" + std::to_string( __LINE__ ) + \
                " while evaluating " + #X ); \
        } \
    } while ( false )
struct BimapTest {
    TEST(basic) {
        Bimap< int, int > a;
        Bimap< int, long > b;

        a.insert( 1, 42 );
        b.insert( 1, 42 );

        Bimap< int, int > ac( a );
        Bimap< int, long > bc( b );

        Bimap< int, int > am( std::move( a ) );
        Bimap< int, long > bm( std::move( b ) );

        TC( ASSERT_EQ( am.left()[ 1 ], 42L ) );
        TC( ASSERT_EQ( am.right()[ 42 ], 1 ) );
        TC( ASSERT_EQ( bm.left()[ 1 ], 42L ) );
        TC( ASSERT_EQ( bm.right()[ 42 ], 1 ) );

        Bimap< int, int > ai{ { 1, 42 }, { 2, 3 } };
        Bimap< int, long > bi{ { 1, 42 }, { 2, 3 } };

        TC( ASSERT_EQ( ai.left()[ 1 ], 42L ) );
        TC( ASSERT_EQ( ai.right()[ 42 ], 1 ) );
        TC( ASSERT_EQ( ai.left()[ 2 ], 3L ) );
        TC( ASSERT_EQ( ai.right()[ 3 ], 2 ) );

        TC( ASSERT_EQ( bi[ 1 ], 42L ) );
        TC( ASSERT_EQ( bi[ 42L ], 1 ) );
        TC( ASSERT_EQ( bi[ 2 ], 3L ) );
        TC( ASSERT_EQ( bi[ 3L ], 2 ) );

        Bimap< int, int > ait( ac.left().begin(), ac.left().end() );
        Bimap< int, long > bit( bc.left().begin(), bc.left().end() );

        TC( ASSERT_EQ( ait.left()[ 1 ], 42L ) );
        TC( ASSERT_EQ( ait.right()[ 42 ], 1 ) );
        TC( ASSERT_EQ( bit[ 1 ], 42L ) );
        TC( ASSERT_EQ( bit[ 42L ], 1 ) );
    }

    TEST(outOfRange) {
        Bimap< int, long > bim{ { 1, 42 } };
        TC( bim[ 42L ] );
        TC( bim[ 1 ] );
        try {
            bim[ 42 ];
            ASSERT( false );
        } catch ( std::out_of_range & ) { }
        try {
            bim[ 1L ];
            ASSERT( false );
        } catch ( std::out_of_range & ) { }
    }

    TEST(bijection) {
        Bimap< int, long > bim;
        bim.insert( 1, 42 );
        try {
            ASSERT( bim.insert( 1, 1 ) && false );
        } catch ( std::invalid_argument & ) { }
        ASSERT_EQ( bim.left().size(), 1ul );
        ASSERT_EQ( bim.right().size(), 1ul );
        try {
            ASSERT( bim.insert( 2, 42 ) && false );
        } catch ( std::invalid_argument & ) { }
        ASSERT_EQ( bim.left().size(), 1ul );
        ASSERT_EQ( bim.right().size(), 1ul );
    }

    TEST(bijection_ctor) {
        try {
            Bimap< int, int > bim{ { 1, 1 }, { 1, 42 } };
            ASSERT( false );
        } catch ( std::invalid_argument & ) { }
        try {
            Bimap< int, int > bim{ { 2, 42 }, { 1, 42 } };
            ASSERT( false );
        } catch ( std::invalid_argument & ) { }
    }
};

struct TestSmallVector {

    struct A {
        A( int &v ) : v( &v ) {
            v = 1;
        }
        ~A() {
            *v = 2;
        }
        int *v;
    };

    TEST(basic) {
        SmallVector< long > vec_l;
        vec_l.emplace_back( 1 );
        ASSERT_EQ( vec_l[ 0 ], 1 );
        vec_l.emplace_back( 2 );
        ASSERT_EQ( vec_l[ 1 ], 2 );
        vec_l.emplace_back( 3 ); vec_l.emplace_back( 4 ); vec_l.emplace_back( 5 );
        ASSERT_EQ( vec_l[ 4 ], 5 );
        ASSERT_EQ( vec_l.end()[ -1 ], 5 );
        SmallVector< int > vec_i;
        for ( int i = 1; i < 128; ++i ) {
            vec_i.emplace_back( i );
            ASSERT_EQ( i, vec_i.size() );
            ASSERT_LEQ( i, vec_i.capacity() );
        }
        int v = 0;
        {
            SmallVector< A > vec_a;
            vec_a.emplace_back( v );
            ASSERT_EQ( v, 1 );
        }
        ASSERT_EQ( v, 2 );
    }

    TEST(resize) {
        SmallVector< int, 8 > vec;
        ASSERT_EQ( vec.capacity(), 8 );
        ASSERT_EQ( vec.size(), 0 );
        vec.resize( 14 );
        ASSERT_EQ( vec.capacity(), 16 );
        ASSERT_EQ( vec.size(), 14 );
        vec.resize( 200 );
        ASSERT_EQ( vec.capacity(), 256 );
        ASSERT_EQ( vec.size(), 200 );
        vec.resize( 512 );
        ASSERT_EQ( vec.capacity(), 512 );
        ASSERT_EQ( vec.size(), 512 );
        vec.resize( 16 );
        ASSERT_EQ( vec.capacity(), 512 );
        ASSERT_EQ( vec.size(), 16 );
        vec[ 0 ] = 42;
        vec.resize( 1 );
        ASSERT_EQ( vec.capacity(), 512 );
        ASSERT_EQ( vec.size(), 1 );
        ASSERT_EQ( vec[ 0 ], 42 );
        vec.resize( 0 );
        ASSERT_EQ( vec.capacity(), 512 );
        ASSERT_EQ( vec.size(), 0 );
    }

    struct Tracker {
        Tracker( int &cnt ) : _cnt( &cnt ) { ++(*_cnt); }
        Tracker( const Tracker &o ) : _cnt( o._cnt ) { ++(*_cnt); }
        ~Tracker() { --(*_cnt); }
        int *_cnt;
    };

    TEST(resize_copy) {
        SmallVector< Tracker, 4 > vec;
        int cnt = 0;
        vec.resize( 16, Tracker( cnt ) );
        ASSERT_EQ( vec.size(), 16 );
        ASSERT_EQ( vec.capacity(), 16 );
        ASSERT_EQ( cnt, 16 );
        vec.resize( 24, Tracker( cnt ) );
        ASSERT_EQ( vec.size(), 24 );
        ASSERT_EQ( vec.capacity(), 32 );
        ASSERT_EQ( cnt, 24 );
        vec.resize( 8, Tracker( cnt ) );
        ASSERT_EQ( vec.size(), 8 );
        ASSERT_EQ( vec.capacity(), 32 );
        ASSERT_EQ( cnt, 8 );
        vec.clear();
        ASSERT_EQ( vec.size(), 0 );
        ASSERT_EQ( vec.capacity(), 32 );
        ASSERT_EQ( cnt, 0 );
        vec.clear_and_drop_memory();
        ASSERT_EQ( vec.size(), 0 );
        ASSERT_EQ( vec.capacity(), 4 );
        vec.resize( 2, Tracker( cnt ) );
        ASSERT_EQ( vec.size(), 2 );
        ASSERT_EQ( vec.capacity(), 4 );
        ASSERT_EQ( cnt, 2 );
        vec.clear_and_drop_memory();
        ASSERT_EQ( vec.size(), 0 );
        ASSERT_EQ( vec.capacity(), 4 );
    }

    TEST(reserve) {
        SmallVector< int, 8 > vec;
        ASSERT_EQ( vec.capacity(), 8 );
        ASSERT_EQ( vec.size(), 0 );
        vec.reserve( 14 );
        ASSERT_EQ( vec.capacity(), 16 );
        ASSERT_EQ( vec.size(), 0 );
        vec.reserve( 200 );
        ASSERT_EQ( vec.capacity(), 256 );
        ASSERT_EQ( vec.size(), 0 );
        vec.reserve( 512 );
        ASSERT_EQ( vec.capacity(), 512 );
        ASSERT_EQ( vec.size(), 0 );
        vec.reserve( 16 );
        ASSERT_EQ( vec.capacity(), 512 );
        ASSERT_EQ( vec.size(), 0 );
        vec.reserve( 0 );
        ASSERT_EQ( vec.capacity(), 512 );
        ASSERT_EQ( vec.size(), 0 );
    }

    template< int def > struct Int {
        Int( int i ) : v( i ) { }
        Int() : v( def ) { }
        ~Int() { v = 0; }
        operator int() { return v; }
        int v;
    };

    TEST(defcon) {
        SmallVector< Int< 42 >, 8 > vec;
        std::fill( vec.data(), vec.data() + 8, 0 );

        vec.resize( 4 );
        for ( int i = 0; i < 4; ++i )
            ASSERT_EQ( int( vec[ i ] ), 42 );
        for ( int i = 4; i < 8; ++i )
            ASSERT_EQ( int( vec[ i ] ), 0 );

        vec.resize( 16 );
        for ( int i = 0; i < 16; ++i )
            ASSERT_EQ( int( vec[ i ] ), 42 );

        vec.resize( 8 );
        for ( int i = 0; i < 8; ++i )
            ASSERT_EQ( int( vec[ i ] ), 42 );
        for ( int i = 8; i < 16; ++i )
            ASSERT_EQ( int( vec[ i ] ), 0 );

        vec.clear();
        for ( int i = 0; i < 16; ++i )
            ASSERT_EQ( int( vec[ i ] ), 0 );
    }

    TEST(push) {
        SmallVector< int > vec;
        for ( int i = 0; i < 512; ++i ) {
            vec.emplace_back( i );
            ASSERT_EQ( vec.back(), i );
            ASSERT_EQ( vec.size(), i + 1 );
            ASSERT_LEQ( i + 1, vec.capacity() );
        }
        for ( int i = 511; i >= 256; --i ) {
            ASSERT_EQ( vec.back(), i );
            vec.pop_back();
            ASSERT_EQ( vec.size(), i );
            ASSERT_EQ( vec.capacity(), 512 );
        }
        for ( int i = 256; i < 512; ++i ) {
            vec.emplace_back( i );
            ASSERT_EQ( vec.back(), i );
            ASSERT_EQ( vec.size(), i + 1 );
            ASSERT_EQ( vec.capacity(), 512 );
        }
        for ( int i = 0; i < 512; ++i )
            ASSERT_EQ( vec[ i ], i );
        ASSERT_EQ( vec.size(), 512 );
        ASSERT_EQ( vec.capacity(), 512 );
        while ( !vec.empty() ) {
            ASSERT_LEQ( 1, vec.size() );
            vec.pop_back();
            ASSERT_LEQ( 0, vec.size() );
        }
        ASSERT_EQ( 0, vec.size() );
    }
};

#undef TC
}
}

#endif // BRICK_DATA_H

// vim: syntax=cpp tabstop=4 shiftwidth=4 expandtab
