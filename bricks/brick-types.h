// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * Assorted types, mosly for C++11.
 * - Maybe a = Just a | Nothing (w/ a limited variant for C++98)
 * - Unit: single-valued type (empty structure)
 * - Union: distriminated (tagged) union
 * - StrongEnumFlags
 */

/*
 * (c) 2006, 2014 Petr Ročkai <me@mornfall.net>
 * (c) 2013-2014 Vladimír Štill <xstill@fi.muni.cz>
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

#include <brick-unittest.h>

#include <memory>
#include <cstring>
#include <type_traits>

#ifndef BRICK_TYPES_H
#define BRICK_TYPES_H

#if __cplusplus >= 201103L
#define CONSTEXPR constexpr
#else
#define CONSTEXPR
#endif

#if __cplusplus > 201103L
#define CPP1Y_CONSTEXPR constexpr // C++1y
#else
#define CPP1Y_CONSTEXPR // C++11
#endif

namespace brick {
namespace types {

struct Unit {
    bool operator<( Unit ) const { return false; }
    bool operator==( Unit ) const { return true; }
};

struct Preferred { CONSTEXPR Preferred() { } };
struct NotPreferred { CONSTEXPR NotPreferred( Preferred ) {} };

struct Comparable {
    typedef bool IsComparable;
};

template< typename T >
typename T::IsComparable operator!=( const T &a, const T &b ) {
    return not( a == b );
}

template< typename T >
typename T::IsComparable operator==( const T &a, const T &b ) {
    return a <= b && b <= a;
}

template< typename T >
typename T::IsComparable operator<( const T &a, const T &b ) {
    return a <= b && a != b;
}

template< typename T >
typename T::IsComparable operator>( const T &a, const T &b ) {
    return b <= a && a != b;
}

template< typename T >
typename T::IsComparable operator>=( const T &a, const T &b ) {
    return b <= a;
}

namespace mixin {

#if __cplusplus >= 201103L
template< typename Self >
struct LexComparable {
    const Self &lcSelf() const { return *static_cast< const Self * >( this ); }

    bool operator==( const Self &o ) const {
        return lcSelf().toTuple() == o.toTuple();
    }

    bool operator!=( const Self &o ) const {
        return lcSelf().toTuple() != o.toTuple();
    }

    bool operator<( const Self &o ) const {
        return lcSelf().toTuple() < o.toTuple();
    }

    bool operator<=( const Self &o ) const {
        return lcSelf().toTuple() <= o.toTuple();
    }

    bool operator>( const Self &o ) const {
        return lcSelf().toTuple() > o.toTuple();
    }

    bool operator>=( const Self &o ) const {
        return lcSelf().toTuple() >= o.toTuple();
    }
};
#endif

}

#if __cplusplus < 201103L

/*
  A Maybe type. Values of type Maybe< T > can be either Just T or Nothing.

  Maybe< int > foo;
  foo = Maybe::Nothing();
  // or
  foo = Maybe::Just( 5 );
  if ( !foo.nothing() ) {
    int real = foo;
  } else {
    // we haven't got anythig in foo
  }

  Maybe takes a default value, which is normally T(). That is what you
  get if you try to use Nothing as T.
*/

template <typename T>
struct Maybe : Comparable {
    bool nothing() const { return m_nothing; }
    bool isNothing() const { return nothing(); }
    T &value() { return m_value; }
    const T &value() const { return m_value; }
    Maybe( bool n, const T &v ) : m_nothing( n ), m_value( v ) {}
    Maybe( const T &df = T() )
       : m_nothing( true ), m_value( df ) {}
    static Maybe Just( const T &t ) { return Maybe( false, t ); }
    static Maybe Nothing( const T &df = T() ) {
        return Maybe( true, df ); }
    operator T() const { return value(); }

    bool operator <=( const Maybe< T > &o ) const {
        if (o.nothing())
            return true;
        if (nothing())
            return false;
        return value() <= o.value();
    }
protected:
    bool m_nothing:1;
    T m_value;
};

#else

template< typename T >
struct StorableRef {
    T _t;
    T &t() { return _t; }
    const T &t() const { return _t; }
    StorableRef( T t ) : _t( t ) {}
};

template< typename T >
struct StorableRef< T & > {
    T *_t;
    T &t() { return *_t; }
    const T &t() const { return *_t; }
    StorableRef( T &t ) : _t( &t ) {}
};

template< typename _T >
struct Maybe : Comparable
{
    using T = _T;

    bool isNothing() const { return _nothing; }
    bool isJust() const { return !_nothing; }

    T &value() {
        ASSERT( isJust() );
        return _v.t.t();
    }

    const T &value() const {
        ASSERT( isJust() );
        return _v.t.t();
    }

    static Maybe Just( const T &t ) { return Maybe( t ); }
    static Maybe Nothing() { return Maybe(); }

    Maybe( const Maybe &m ) {
        _nothing = m.isNothing();
        if ( !_nothing )
            _v.t = m._v.t;
    }

    ~Maybe() {
        if ( !_nothing )
            _v.t.~StorableRef< T >();
    }

    bool operator <=( const Maybe< T > &o ) const {
        if (o.isNothing())
            return true;
        if (isNothing())
            return false;
        return value() <= o.value();
    }

protected:

    Maybe( const T &v ) : _v( v ), _nothing( false ) {}
    Maybe() : _nothing( true ) {}
    struct Empty {
        char x[ sizeof( T ) ];
    };

    union V {
        StorableRef< T > t;
        Empty empty;
        V() : empty() {}
        V( const T &t ) : t( t ) {}
        ~V() { } // see dtor of Maybe
    };
    V _v;
    bool _nothing;
};

#endif

template<>
struct Maybe< void > {
    typedef void T;
    static Maybe Just() { return Maybe( false ); }
    static Maybe Nothing() { return Maybe( true ); }
    bool isNothing() { return _nothing; }
    bool isJust() { return !_nothing; }
private:
    Maybe( bool nothing ) : _nothing( nothing ) {}
    bool _nothing;
};

#if __cplusplus >= 201103L

template< typename E >
using is_enum_class = std::integral_constant< bool,
        std::is_enum< E >::value && !std::is_convertible< E, int >::value >;

template< typename Self >
struct StrongEnumFlags {
    static_assert( is_enum_class< Self >::value, "Not an enum class." );
    using This = StrongEnumFlags< Self >;
    using UnderlyingType = typename std::underlying_type< Self >::type;

    constexpr StrongEnumFlags() noexcept : store( 0 ) { }
    constexpr StrongEnumFlags( Self flag ) noexcept :
        store( static_cast< UnderlyingType >( flag ) )
    { }
    explicit constexpr StrongEnumFlags( UnderlyingType st ) noexcept : store( st ) { }

    constexpr explicit operator UnderlyingType() const noexcept {
        return store;
    }

    This &operator|=( This o ) noexcept {
        store |= o.store;
        return *this;
    }

    This &operator&=( This o ) noexcept {
        store &= o.store;
        return *this;
    }

    This &operator^=( This o ) noexcept {
        store ^= o.store;
        return *this;
    }

    friend constexpr This operator|( This a, This b ) noexcept {
        return This( a.store | b.store );
    }

    friend constexpr This operator&( This a, This b ) noexcept {
        return This( a.store & b.store );
    }

    friend constexpr This operator^( This a, This b ) noexcept {
        return This( a.store ^ b.store );
    }

    friend constexpr bool operator==( This a, This b ) noexcept {
        return a.store == b.store;
    }

    friend constexpr bool operator!=( This a, This b ) noexcept {
        return a.store != b.store;
    }

    constexpr bool has( Self x ) const noexcept {
        return (*this) & x;
    }

    constexpr operator bool() const noexcept {
        return store;
    }

  private:
    UnderlyingType store;
};

// don't catch integral types and classical enum!
template< typename Self, typename = typename
          std::enable_if< is_enum_class< Self >::value >::type >
constexpr StrongEnumFlags< Self > operator|( Self a, Self b ) noexcept {
    using Ret = StrongEnumFlags< Self >;
    return Ret( a ) | Ret( b );
}

template< typename Self, typename = typename
          std::enable_if< is_enum_class< Self >::value >::type >
constexpr StrongEnumFlags< Self > operator&( Self a, Self b ) noexcept {
    using Ret = StrongEnumFlags< Self >;
    return Ret( a ) & Ret( b );
}

/* implementation of Union */

namespace _impl {
    template< size_t val, typename... >
    struct MaxSizeof : std::integral_constant< size_t, val > { };

    template< size_t val, typename T, typename... Ts >
    struct MaxSizeof< val, T, Ts... > :
        MaxSizeof< ( val > sizeof( T ) ) ? val : sizeof( T ), Ts... >
    { };

    template< size_t val, typename... >
    struct MaxAlign : std::integral_constant< size_t, val > { };

    template< size_t val, typename T, typename... Ts >
    struct MaxAlign< val, T, Ts... > :
        MaxAlign< ( val > std::alignment_of< T >::value )
                      ? val : std::alignment_of< T >::value, Ts... >
    { };

    template< typename... >
    struct AllDistinct : std::true_type { };

    template< typename, typename... >
    struct In : std::false_type { };

    template< typename Needle, typename T, typename... Ts >
    struct In< Needle, T, Ts... > : std::integral_constant< bool,
        std::is_same< Needle, T >::value || In< Needle, Ts... >::value >
    { };

    template< typename _T >
    struct Witness { using T = _T; };

    template< typename, typename... >
    struct _OneConversion { };

    template< typename From, typename To, typename... >
    struct NoneConvertible { using T = To; };

    template< typename From, typename To, typename T, typename... Ts >
    struct NoneConvertible< From, To, T, Ts... > : std::conditional<
        std::is_convertible< From, T >::value,
        Unit,
        NoneConvertible< From, To, Ts... > >::type { };

    static_assert( std::is_convertible< Witness< int >, Witness< int > >::value, "is_convertible" );

    template< typename Needle, typename T, typename... Ts >
    struct _OneConversion< Needle, T, Ts... > : std::conditional<
        std::is_convertible< Needle, T >::value,
        NoneConvertible< Needle, T, Ts... >,
        _OneConversion< Needle, Ts... > >::type { };

    template< typename Needle, typename... Ts >
    struct OneConversion : std::conditional<
        In< Needle, Ts... >::value,
        Witness< Needle >,
        _OneConversion< Needle, Ts... > >::type { };

    static_assert( std::is_same< OneConversion< int, int >::T, int >::value, "OneConversion" );
    static_assert( std::is_same< OneConversion< long, int >::T, int >::value, "OneConversion" );
    static_assert( std::is_same< OneConversion< long, std::string, int >::T, int >::value, "OneConversion" );
    static_assert( std::is_same< OneConversion< long, int, long, int >::T, long >::value, "OneConversion" );

    template< typename T, typename... Ts >
    struct AllDistinct< T, Ts... > : std::integral_constant< bool,
        !In< T, Ts... >::value && AllDistinct< Ts... >::value >
    { };

template< typename F, typename T, typename Fallback, typename Check = bool >
struct _ApplyResult : Fallback {};

template< typename F, typename T, typename Fallback >
struct _ApplyResult< F, T, Fallback, decltype( std::declval< F >()( std::declval< T >() ), true ) >
{
    using Parameter = T;
    using Result = decltype( std::declval< F >()( std::declval< T >() ) );
};

template< typename F, typename... Ts > struct ApplyResult;

template< typename F, typename T, typename... Ts >
struct ApplyResult< F, T, Ts... > : _ApplyResult< F, T, ApplyResult< F, Ts... > > {};

template< typename F > struct ApplyResult< F > {};

}

template< typename T >
struct InPlace { };

struct NullUnion { };

template< typename... Types >
struct Union {
    static_assert( sizeof...( Types ) < 0xff, "Too much unioned types, sorry" );
    static_assert( _impl::AllDistinct< Types... >::value,
            "All types in union must be distinct" );

    constexpr Union() : discriminator( 0 ) { }
    constexpr Union( NullUnion ) : discriminator( 0 ) { }

    Union( const Union &other ) {
        ASSERT_LEQ( size_t( other.discriminator ), sizeof...( Types ) );
        if ( other.discriminator > 0 )
            _copyConstruct< 1, Types... >( other.discriminator, other );
        discriminator = other.discriminator;
    }

    Union( Union &&other ) {
        ASSERT_LEQ( size_t( other.discriminator ), sizeof...( Types ) );
        auto target = other.discriminator;
        other.discriminator = 0;
        if ( target > 0 )
            _moveConstruct< 1, Types... >( target, std::move( other ) );
        discriminator = target;
    }

    template< typename T, typename U = typename _impl::OneConversion< T, Types... >::T >
    CPP1Y_CONSTEXPR Union( T val ) {
        new ( &storage ) U( std::move( val ) );
        discriminator = _discriminator< U >();
    }

    template< typename T, typename... Args >
    Union( InPlace< T >, Args &&... args ) : discriminator( _discriminator< T >() ) {
        new ( &storage ) T( std::forward< Args >( args )... );
    }

    // use copy and swap
    Union &operator=( Union other ) {
        swap( other );
        return *this;
    }

    template< typename T >
    auto operator=( const T &other ) -> typename
        std::enable_if< std::is_lvalue_reference< T & >::value, Union & >::type
    {
        if ( is< T >() )
            unsafeGet< T >() = other;
        else
            _copyAssignDifferent( Union( other ) );
        return *this;
    }

    template< typename T >
    auto operator=( T &&other ) -> typename
        std::enable_if< std::is_rvalue_reference< T && >::value, Union & >::type
    {
        if ( is< T >() )
            unsafeGet< T >() = std::move( other );
        else
            _moveAssignDifferent( std::move( other ) );
        return *this;
    }

    void swap( Union other ) {
        typename std::aligned_storage< size, algignment >::type tmpStor;
        unsigned char tmpDis;

        std::memcpy( &tmpStor, &other.storage, size );
        tmpDis = other.discriminator;
        other.discriminator = 0;
        std::memcpy( &other.storage, &storage, size );
        other.discriminator = discriminator;
        discriminator = 0;
        std::memcpy( &storage, &tmpStor, size );
        discriminator = tmpDis;
    }

    bool empty() {
        return discriminator == 0;
    }

    explicit operator bool() {
        return discriminator > 0;
    }

    template< typename T >
    bool is() const {
        return _discriminator< T >() == discriminator;
    }

    template< typename T >
    operator T() {
        return get< T >();
    }

    template< typename T >
    T &get() {
        ASSERT( is< T >() );
        return unsafeGet< T >();
    }

    template< typename T >
    const T &get() const {
        return cget< T >();
    }

    template< typename T >
    const T &cget() const {
        ASSERT( is< T >() );
        return unsafeGet< T >();
    }

    template< typename T >
    const T &getOr( const T &val ) const {
        if ( is< T >() )
            return unsafeGet< T >();
        return val;
    }

    template< typename T >
    T &unsafeGet() {
        return *reinterpret_cast< T * >( &storage );
    }

    template< typename T >
    const T &unsafeGet() const {
        return *reinterpret_cast< const T * >( &storage );
    }

    template< typename T >
    T &&moveOut() {
        ASSERT( is< T >() );
        return unsafeMoveOut< T >();
    }

    template< typename T >
    T &&unsafeMoveOut() {
        return std::move( *reinterpret_cast< T * >( &storage ) );
    }

    template< typename F >
    using Applied = Maybe< typename _impl::ApplyResult< F, Types... >::Result >;

    // invoke `f` on the stored value if the type currently stored in the union
    // can be legally passed to that function as an argument
    template< typename F >
    auto apply( F f ) -> Applied< F > {
        return _apply< F, Types... >( Preferred(), f );
    }

    template< typename R >
    R _match() { return R::Nothing(); }

    // invoke the first function that can handle the currently stored value
    // (type-based pattern matching)
    template< typename R, typename F, typename... Args >
    R _match( F f, Args... args ) {
        auto x = apply( f );
        if ( x.isNothing() )
            return _match< R >( args... );
        else
            return x;
    }

    template< typename F, typename... Args >
    Applied< F > match( F f, Args... args ) {
        return _match< Applied< F > >( f, args... );
    }

  private:
    static constexpr size_t size = _impl::MaxSizeof< 1, Types... >::value;
    static constexpr size_t algignment = _impl::MaxAlign< 1, Types... >::value;
    typename std::aligned_storage< size, algignment >::type storage;
    unsigned char discriminator;

    template< typename T >
    unsigned char _discriminator() const {
        static_assert( _impl::In< T, Types... >::value,
                "Trying to construct Union from value of type not allowed for it." );
        return _discriminator< 1, T, Types... >();
    }

    template< unsigned char i, typename Needle, typename T, typename... Ts >
    constexpr unsigned char _discriminator() const {
        return std::is_same< Needle, T >::value
            ? i : _discriminator< i + 1, Needle, Ts... >();
    }

    template< unsigned char, typename >
    constexpr unsigned char _discriminator() const { return 0; /* cannot happen */ }

    template< unsigned char i, typename T, typename... Ts >
    void _copyConstruct( unsigned char d, const Union &other ) {
        if ( i == d )
            new ( &storage ) T( other.unsafeGet< T >() );
        else
            _copyConstruct< i + 1, Ts... >( d, other );
    }

    template< unsigned char >
    unsigned char _copyConstruct( unsigned char, const Union & )
    { ASSERT_UNREACHABLE( "invalid _copyConstruct" ); }

    template< unsigned char i, typename T, typename... Ts >
    void _moveConstruct( unsigned char d, Union &&other ) {
        if ( i == d )
            new ( &storage ) T( other.unsafeMoveOut< T >() );
        else
            _moveConstruct< i + 1, Ts... >( d, std::move( other ) );
    }

    template< unsigned char >
    unsigned char _moveConstruct( unsigned char, Union && )
    { ASSERT_UNREACHABLE( "invalid _moveConstruct" ); }

    void _copyAssignDifferent( const Union &other ) {
        auto tmp = discriminator;
        discriminator = 0;
        if ( tmp )
            _destruct< 1, Types... >( tmp );
        if ( other.discriminator )
            _copyConstruct< 1, Types... >( other.discriminator, other );
        discriminator = other.discriminator;
    }

    void _copyAssignSame( const Union &other ) {
        ASSERT_EQ( discriminator, other.discriminator );
        if ( discriminator == 0 )
            return;
        _copyAssignSame< 1, Types... >( other );
    }

    template< unsigned char i, typename T, typename... Ts >
    void _copyAssignSame( const Union &other ) {
        if ( i == discriminator )
            unsafeGet< T >() = other.unsafeGet< T >();
        else
            _copyAssignSame< i + 1, Ts... >( other );
    }

    template< unsigned char >
    void _copyAssignSame( const Union & ) { ASSERT_UNREACHABLE( "invalid _copyAssignSame" ); }

    template< unsigned char i, typename T, typename... Ts >
    void _destruct( unsigned char d ) {
        if ( i == d )
            unsafeGet< T >().~T();
        else
            _destruct< i + 1, Ts... >( d );
    }

    template< unsigned char >
    void _destruct( unsigned char ) { ASSERT_UNREACHABLE( "invalid _destruct" ); }

    void _moveAssignSame( Union &&other ) {
        ASSERT_EQ( discriminator, other.discriminator );
        if ( discriminator == 0 )
            return;
        _moveAssignSame< 1, Types... >( std::move( other ) );
    }

    template< unsigned char i, typename T, typename... Ts >
    void _moveAssignSame( Union &&other ) {
        if ( i == discriminator )
            unsafeGet< T >() = other.unsafeMoveOut< T >();
        else
            _moveAssignSame< i + 1, Ts... >( std::move( other ) );
    }

    template< unsigned char >
    void _moveAssignSame( Union && ) { ASSERT_UNREACHABLE( "invalid _moveAssignSame" ); }

    void _moveAssignDifferent( Union &&other ) {
        auto tmp = discriminator;
        auto target = other.discriminator;
        discriminator = 0;
        if ( tmp )
            _destruct< 1, Types... >( tmp );
        if ( target )
            _moveConstruct< 1, Types... >( target, std::move( other ) );
        discriminator = target;
    }

    template< typename F > Applied< F > _apply( Preferred, F ) { return Applied< F >::Nothing(); }

    template< typename F, typename T >
    auto fixvoid( F f ) ->
        typename std::enable_if< std::is_void< typename Applied< F >::T >::value, Applied< F > >::type
    {
        f( get< T >() );
        return Maybe< void >::Just();
    }

    template< typename F, typename T >
    auto fixvoid( F f ) ->
        typename std::enable_if< !std::is_void< typename Applied< F >::T >::value, Applied< F > >::type
    {
        return Applied< F >::Just( f( get< T >() ) );
    }

    template< typename F, typename T, typename... Args >
    auto _apply( Preferred, F f ) -> Maybe< typename _impl::_ApplyResult< F, T, Unit >::Result >
    {
        if ( !is< T >() )
            return _apply< F, Args... >( Preferred(), f );

        return fixvoid< F, T >( f );
    }

    template< typename F, typename T, typename... Args >
    auto _apply( NotPreferred, F f ) -> Applied< F >
    {
        return _apply< F, Args... >( Preferred(), f );
    }

};

#endif // C++11

}
}

namespace brick_test {
namespace types {

using namespace ::brick::types;

struct Integer : Comparable
{
    int val;
public:
    Integer(int val) : val(val) {}
    bool operator<=( const Integer& o ) const { return val <= o.val; }
};

struct Mixins {

    TEST(comparable) {
        Integer i10(10);
        Integer i10a(10);
        Integer i20(20);

        ASSERT(i10 <= i10a);
        ASSERT(i10a <= i10);
        ASSERT(i10 <= i20);
        ASSERT(! (i20 <= i10));

        ASSERT(i10 != i20);
        ASSERT(!(i10 != i10a));

        ASSERT(i10 == i10a);
        ASSERT(!(i10 == i20));

        ASSERT(i10 < i20);
        ASSERT(!(i20 < i10));
        ASSERT(!(i10 < i10a));

        ASSERT(i20 > i10);
        ASSERT(!(i10 > i20));
        ASSERT(!(i10 > i10a));

        ASSERT(i10 >= i10a);
        ASSERT(i10a >= i10);
        ASSERT(i20 >= i10);
        ASSERT(! (i10 >= i20));
    }

};

#if __cplusplus >= 201103L

struct A { };
struct B { B() { }; ~B() { } };
struct C { int x; C( int x ) : x( x ) {} C() : x( 0 ) {} };

static_assert( _impl::In< int, int >::value, "" );
static_assert( _impl::In< A, A, B >::value, "" );
static_assert( _impl::In< A, B, A >::value, "" );

// test instances
struct UnionInstances {
    Union<> a;
    Union< int, long > b;
    Union< int, long, A > c;
    Union< int, long, A, B > d;
    Union< int, long, A, B, std::string > e;
};

struct UnionTest {
    TEST(basic) {
        Union< int > u( 1 );
        ASSERT( !!u );
        ASSERT( !u.empty() );
        ASSERT( u.is< int >() );
        ASSERT_EQ( u.get< int >(), 1 );
        u = 2; // move
        ASSERT( !!u );
        ASSERT_EQ( u.get< int >(), 2 );
        int i = 3;
        u = i; // copy
        ASSERT( !!u );
        ASSERT_EQ( u.get< int >(), 3 );
        u = types::Union< int >( 4 );
        ASSERT( u.is< int >() );
        ASSERT_EQ( u.get< int >(), 4 );
        u = types::Union< int >();
        ASSERT( !u );
        ASSERT( !u.is< int >() );
        u = 5;
        ASSERT( u );
        ASSERT( u.is< int >() );
        ASSERT_EQ( u.get< int >(), 5 );
    }

    TEST(moveNoCopy) {
        // if one of contained structures does not define copy ctor+assignment
        // move should still be available
        struct Move {
            Move() = default;
            Move( const Move & ) = delete;
            Move( Move && ) = default;

            Move &operator=( Move ) { return *this; }
        };
        Union< long, Move > wierd;
        ASSERT( wierd.empty() );

        wierd = 2L;
        ASSERT( !!wierd );
        ASSERT( wierd.is< long >() );
        ASSERT_EQ( wierd.get< long >(), 2L );

        wierd = Move();
        ASSERT( !!wierd );
        ASSERT( wierd.is< Move >() );
    }

    TEST(ctorCast) {
        ASSERT( ( Union< int, long >{ int( 1 ) }.is< int >() ) );
        ASSERT( ( Union< int, long >{ long( 1 ) }.is< long >() ) );

        ASSERT( ( Union< long, std::string >{ int( 1 ) }.is< long >() ) );

        struct A { operator int(){ return 1; } };
        ASSERT( ( Union< int, A >{ A() }.is< A >() ) );
        ASSERT( ( Union< int, std::string >{ A() }.is< int >() ) );

        struct B { B( int ) { } B() = default; };
        ASSERT( ( Union< int, B >{ B() }.is< B >() ) );
        ASSERT( ( Union< int, B >{ 1 }.is< int >() ) );
        ASSERT( ( Union< B, std::string >{ 1 }.is< B >() ) );
    }

    static C idC( C c ) { return c; };
    static C constC( B b ) { return C( 32 ); };

    TEST(apply) {
        Union< B, C > u;
        u = B();

        Maybe< C > result = u.match( idC, constC );
        ASSERT( !result.isNothing() );
        ASSERT_EQ( result.value().x, 32 );

        u = C( 12 );
        result = u.match( idC, constC );
        ASSERT( !result.isNothing() );
        ASSERT_EQ( result.value().x, 12 );

        result = u.match( constC );
        ASSERT( result.isNothing() );
    }
};

enum class FA : unsigned char  { X = 1, Y = 2, Z = 4 };
enum class FB : unsigned short { X = 1, Y = 2, Z = 4 };
enum class FC : unsigned       { X = 1, Y = 2, Z = 4 };
enum class FD : unsigned long  { X = 1, Y = 2, Z = 4 };

struct StrongEnumFlagsTest {
    template< typename Enum >
    void testEnum() {
        StrongEnumFlags< Enum > e1;
        StrongEnumFlags< Enum > e2( Enum::X );

        ASSERT( !e1 );
        ASSERT( e2 );

        ASSERT( e1 | e2 );
        ASSERT( Enum::X | Enum::Y );
        ASSERT( e2 | Enum::Z );
        ASSERT( e2.has( Enum::X ) );

        ASSERT( e2 & Enum::X );
        ASSERT( !( Enum::X & Enum::Y ) );

        ASSERT( Enum::X | Enum::Y | Enum::Z );
        ASSERT( !( Enum::X & Enum::Y & Enum::Z ) );
        ASSERT( ( Enum::X | Enum::Y | Enum::Z ) & Enum::X );
    }

    // we don't want to break classical enums and ints by out operators
    TEST(regression) {
        enum Classic { C_X = 1, C_Y = 2, C_Z = 4 };

        ASSERT( C_X | C_Y | C_Z );
        ASSERT( 1 | 2 | 4 );
        ASSERT( C_X & 1 );
    }

    TEST(enum_uchar) { testEnum< FA >(); }
    TEST(enum_ushort) { testEnum< FB >(); }
    TEST(enum_uint) { testEnum< FC >(); }
    TEST(enum_ulong) { testEnum< FD >(); }
};

#endif

}
}

#endif
// vim: syntax=cpp tabstop=4 shiftwidth=4 expandtab
