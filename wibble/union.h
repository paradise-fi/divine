// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>
/* discriminated union of given objects
 *
 *
 * redistributable under BSD licence
 */

#if __cplusplus < 201103L
#error "union is only supported with c++11 or newer"
#endif

#include <wibble/test.h>
#include <wibble/maybe.h>

#include <memory>
#include <cstring>
#include <type_traits>

#ifndef WIBBLE_UNION_H
#define WIBBLE_UNION_H

#if __cplusplus > 201103L
#define CPP1Y_CONSTEXPR constexpr // C++1y
#else
#define CPP1Y_CONSTEXPR // C++11
#endif

namespace wibble {

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
        wibble::Unit,
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
        assert_leq( size_t( other.discriminator ), sizeof...( Types ) );
        if ( other.discriminator > 0 )
            _copyConstruct< 1, Types... >( other.discriminator, other );
        discriminator = other.discriminator;
    }

    Union( Union &&other ) {
        assert_leq( size_t( other.discriminator ), sizeof...( Types ) );
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
        assert( is< T >() );
        return unsafeGet< T >();
    }

    template< typename T >
    const T &get() const {
        return cget< T >();
    }

    template< typename T >
    const T &cget() const {
        assert( is< T >() );
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
        assert( is< T >() );
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
    { assert_unreachable( "invalid _copyConstruct" ); }

    template< unsigned char i, typename T, typename... Ts >
    void _moveConstruct( unsigned char d, Union &&other ) {
        if ( i == d )
            new ( &storage ) T( other.unsafeMoveOut< T >() );
        else
            _moveConstruct< i + 1, Ts... >( d, std::move( other ) );
    }

    template< unsigned char >
    unsigned char _moveConstruct( unsigned char, Union && )
    { assert_unreachable( "invalid _moveConstruct" ); }

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
        assert_eq( discriminator, other.discriminator );
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
    void _copyAssignSame( const Union & ) { assert_unreachable( "invalid _copyAssignSame" ); }

    template< unsigned char i, typename T, typename... Ts >
    void _destruct( unsigned char d ) {
        if ( i == d )
            unsafeGet< T >().~T();
        else
            _destruct< i + 1, Ts... >( d );
    }

    template< unsigned char >
    void _destruct( unsigned char ) { assert_unreachable( "invalid _destruct" ); }

    void _moveAssignSame( Union &&other ) {
        assert_eq( discriminator, other.discriminator );
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
    void _moveAssignSame( Union && ) { assert_unreachable( "invalid _moveAssignSame" ); }

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

}

#endif // WIBBLE_UNION_H
