#pragma once

#include <iterator>
#include <functional>
#include <utility>
#include <set>

namespace lazy {

template< typename I >
struct Range {
    using iterator = I;
    using value_type = typename I::value_type;
    using reference = typename I::reference;
    using pointer = typename I::pointer;

    Range( I begin, I end ) :
        _begin( begin ),
        _end( end )
    {}

    I begin() const {
        return _begin;
    }
    I end() const {
        return _end;
    }

private:
    I _begin;
    I _end;
};

template< typename T >
struct Optional {

    Optional() :
        _set( false )
    {}
    Optional( const T &value ) :
        _set( true )
    {
        new ( &_u.data ) T( value );
    }
    Optional( const Optional &other ) :
        _set( other._set )
    {
        if ( _set )
            new ( &_u.data ) T( other._u.data );
    }
    Optional &operator=( Optional other ) {
        swap( other );
        return *this;
    }
    ~Optional() {
        if ( _set )
            _u.data.~T();
    }

    explicit operator bool() const noexcept {
        return _set;
    }

    template< typename... Args >
    auto operator()( Args &&... args ) {
        return get()( std::forward< Args >( args )... );
    }
    template< typename... Args >
    auto operator()( Args &&... args ) const {
        return get()( std::forward< Args >( args )... );
    }

    void set( const T &data ) {
        if ( _set )
            _u.data = data;
        else {
            new ( &_u.data ) T( data );
            _set = true;
        }
    }
    T &get() {
        return _u.data;
    }
    const T &get() const {
        return _u.data;
    }

    void swap( Optional &other ) {
        using std::swap;

        if ( _set && other._set ) {
            swap( _u.data, other._u.data );
        }
        else if ( _set ) {
            move( *this, other );
        }
        else if ( other._set ) {
            move( other, *this );
        }
    }

private:

    static void move( Optional &source, Optional &target ) {
        new ( &target._u.data ) T( std::move( source._u.data ) );
        source._u.data.~T();
        source._set = false;
        target._set = true;
    }

    bool _set;
    union U {
        U() {}
        ~U() {}
        T data;
        char padding[ sizeof( T ) ];
    } _u;
};

struct VoidType {};

template< typename >
struct SwallowType {
    using Void = void;
};

template< typename T, typename V = void >
struct IteratorCategory {
    using iterator_category = std::input_iterator_tag;
};

template< typename T >
struct IteratorCategory< T, typename SwallowType< typename T::iterator_category >::Void > {
    using iterator_category = typename std::conditional<
        std::is_base_of< std::forward_iterator_tag, typename T::iterator_category >::value,
        std::forward_iterator_tag,
        std::input_iterator_tag
    >::type;
};

template< typename I >
struct ValueTypeHelper {
private:
    using RV = decltype( *std::declval< I >() );
    using V = typename std::conditional<
        std::is_reference< RV >::value,
        typename std::remove_reference< RV >::type,
        RV
    >::type;
public:
    static constexpr bool IsConst = std::is_const< V >::value;
    using value_type = typename std::remove_cv< V >::type;
};

template< typename P >
struct ValueTypeHelper< P * > {
    static constexpr bool IsConst = std::is_const< P >::value;
    using value_type = typename std::remove_cv< P >::type;
};

template<>
struct ValueTypeHelper< VoidType > {
    static constexpr bool IsConst = false;
    using value_type = VoidType;
};


template< typename I >
struct TypeHelper {
private:
    static constexpr bool IsConst = ValueTypeHelper< I >::IsConst;
public:
    using value_type = typename ValueTypeHelper< I >::value_type;
    using pointer = typename std::conditional<
        IsConst,
        const value_type *,
        value_type *
    >::type;
    using reference = typename std::conditional<
        IsConst,
        const value_type &,
        value_type &
    >::type;
    using difference_type = std::ptrdiff_t;
    using iterator_category = typename IteratorCategory< I >::iterator_category;
};

template< typename Iterator, typename Self >
struct BaseWrapper : TypeHelper< Iterator > {

    BaseWrapper() = default;
    BaseWrapper( Iterator i ) :
        _i( i )
    {}

    BaseWrapper( const BaseWrapper & ) = default;
    BaseWrapper &operator=( const BaseWrapper & ) = default;

    Self &operator++() {
        ++_i;
        return self();
    }
    Self operator++( int ) {
        Self self( this->self() );
        ++this->self();
        return self;
    }

    typename BaseWrapper::reference operator*() const {
        return *_i;
    }
    typename BaseWrapper::pointer operator->() const {
        return _i.operator->();
    }

    bool operator==( const BaseWrapper &other ) const {
        return _i == other._i;
    }
    bool operator!=( const BaseWrapper &other ) const {
        return _i != other._i;
    }

private:
    Self &self() {
        return *static_cast<Self *>( this );
    }
    const Self &self() const {
        return *static_cast<const Self *>( this );
    }
protected:
    Iterator _i;
};


template<
    typename Iterator,
    typename F,
    typename Result = typename std::result_of< F( typename ValueTypeHelper< Iterator >::value_type ) >::type
>
struct MapIterator : BaseWrapper< Iterator, MapIterator< Iterator, F, Result > > {

    using value_type = Result;
    using reference = const Result &;
    using pointer = const Result *;
    using Base = BaseWrapper< Iterator, MapIterator >;

    MapIterator() = default;

    MapIterator( Iterator i, Iterator last, F f ) :
        Base( i ),
        _last( last ),
        _f( f )
    {
        apply();
    }

    MapIterator &operator=( const MapIterator &other ) {
        this->_i = other._i;
        return *this;
    }

    MapIterator &operator++() {
        ++this->_i;
        apply();
        return *this;
    }
    MapIterator operator++( int ) {
        MapIterator self( *this );
        ++( *this );
        return self;
    }

    const value_type &operator*() const {
        return _data.get();
    }
    const value_type *operator->() const {
        return &_data.get();
    }

private:
    void apply() {
        if ( this->_i != _last && _f ) {
            _data.set( _f( *this->_i ) );
        }
    }

    Iterator _last;
    Optional< F > _f;
    Optional< value_type > _data;
};

template< typename I, typename F >
Range< MapIterator< I, F > > map( I first, I last, F f ) {
    return{
        { first, last, f },
        { last, last, f }
    };
}

template< typename Iterator, typename F >
struct Filter : BaseWrapper< Iterator, Filter< Iterator, F > > {

    using Base = BaseWrapper< Iterator, Filter >;

    Filter() = default;
    Filter( Iterator first, Iterator last, F f ) :
        Base( first ),
        _last( last ),
        _f( f )
    {
        bump();
    }

    Filter &operator=( const Filter &other ) {
        this->_i = other._i;
        return *this;
    }

    Filter &operator++() {
        ++this->_i;
        bump();
        return *this;
    }
    Filter operator++( int ) {
        Filter self( *this );
        ++( *this );
        return self;
    }

private:

    void bump() {
        while ( this->_i != _last && _f && !_f( *this->_i ) )
            ++this->_i;
    }

    Iterator _last;
    Optional< F > _f;
};

template< typename I, typename F >
Range< Filter< I, F > > filter( I first, I last, F f ) {
    return{ {first, last, f}, {last, last, f} };
}


template<
    typename I1,
    typename I2,
    typename F,
    typename Result = typename std::result_of< F(
        typename ValueTypeHelper< I1 >::value_type,
        typename ValueTypeHelper< I2 >::value_type )
    >::type
>
struct Zip : TypeHelper< VoidType > {

    using value_type = Result;
    using pointer = const Result *;
    using reference = const Result &;

    Zip( I1 i1, I1 last1, I2 i2, I2 last2, F f ) :
        _i1( i1 ),
        _last1( last1 ),
        _i2( i2 ),
        _last2( last2 ),
        _f( f )
    {
        apply();
    }

    bool operator==( const Zip &other ) const {
        return _i1 == other._i1 && _i2 == other._i2;
    }
    bool operator!=( const Zip &other ) const {
        return !( *this == other );
    }

    reference operator*() const {
        return _data.get();
    }

    pointer operator->() const {
        return &_data.get();
    }

    Zip &operator++() {
        ++_i1;
        ++_i2;
        apply();
        return *this;
    }
    Zip operator++( int ) {
        Zip self( *this );
        ++( *this );
        return self;
    }

private:

    void apply() {
        if ( _i1 != _last1 && _i2 != _last2 && _f ) {
            _data.set( _f( *_i1, *_i2 ) );
        }
        else {
            _i1 = _last1;
            _i2 = _last2;
        }
    }
    I1 _i1;
    I1 _last1;
    I2 _i2;
    I2 _last2;
    Optional< F > _f;
    Optional< Result > _data;
};



template< typename I1, typename I2, typename F >
Range< Zip< I1, I2, F > > zip( I1 first1, I1 last1, I2 first2, I2 last2, F f ) {
    return{
        { first1, last1, first2, last2, f },
        { last1, last1, last2, last2, f }
    };
}

template< typename T >
struct Unique {
    bool operator()( const T &item ) {
        return _seen.insert( item ).second;
    }
private:
    std::set< T > _seen;
};

template< typename I >
Range< Filter< I, Unique< typename ValueTypeHelper< I >::value_type > > > unique( I first, I last ) {
    return filter( first, last, Unique< typename ValueTypeHelper< I >::value_type >() );
}

}


