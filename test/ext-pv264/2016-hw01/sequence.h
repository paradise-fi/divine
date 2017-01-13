#pragma once

#include "lazy.h"

namespace sequence {

template< typename T >
struct Parameter {
    Parameter( size_t n, const T &previous ) :
        n( n ),
        previous( previous )
    {}
    size_t n;
    const T &previous;
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

template<
    typename F,
    typename T
>
struct Iterator : std::iterator< std::forward_iterator_tag, T >
{
    Iterator() = default;
    Iterator( F f ) :
        _f( f ),
        _end( true ),
        _n( 0 ),
        _limit( 0 )
    {}
    Iterator( F f, const T &initial, size_t limit = 0 ) :
        _f( f ),
        _end( false ),
        _n( 0 ),
        _limit( limit ),
        _value( initial )
    {}
    Iterator( const Iterator & ) = default;
    Iterator &operator=( const Iterator &other ) {
        _end = other._end;
        _n = other._n;
        _limit = other._limit;
        _value = other._value;
        return *this;
    }

    Iterator &operator++() {
        ++_n;
        if ( _limit && _n >= _limit )
            _end = true;
        else if ( _f )
            _value.set( _f( Parameter< T >( _n, _value.get() ) ) );
        else
            std::abort();
        return *this;
    }
    Iterator operator++( int ) {
        Iterator self( *this );
        ++( *this );
        return self;
    }

    const T &operator*() const {
        return _value.get();
    }
    const T *operator->() const {
        return &_value.get();
    }

    bool operator==( const Iterator &other ) const {
        if ( _end && other._end )
            return true;
        return
            _end == other._end &&
            _n == other._n &&
            _limit == other._limit;
    }
    bool operator!=( const Iterator &other ) const {
        return !( *this == other );
    }

private:
    Optional< F > _f;
    bool _end;
    size_t _n;
    size_t _limit;
    Optional< T > _value;
};

template< typename F, typename T >
lazy::Range< Iterator< F, T > > finite( F f, const T &initial, size_t limit ) {
    return{
        {f, initial, limit},
        {f}
    };
}

template< typename F, typename T >
lazy::Range< Iterator< F, T > > infinite( F f, const T &initial ) {
    return{
        { f, initial },
        { f }
    };
}


}

