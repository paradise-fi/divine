// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>

#include <memory>
#include <wibble/test.h>
#include <iterator>
#include <algorithm>
#include <initializer_list>

#include <wibble/mixin.h>

#if __cplusplus < 201103L
#error FixArray can be compiled only with C++11
#endif

#ifndef WIBBLE_FIXARRAY
#define WIBBLE_FIXARRAY

namespace wibble {

/* semi-smart dynamic array,
 * it does not grow by itsenf, but can be explicitly resized
 */
template< typename T >
struct FixArray : mixin::Comparable< FixArray< T > > {
    FixArray() : _data( nullptr ), _size( 0 ) { }

    explicit FixArray( ssize_t size ) :
        _data( size ? new T[ size ] : nullptr ),
        _size( size )
    { }
    /*
    FixArray( ssize_t size, T val ) :
        _data( size ? new T[ size ]( val ) : nullptr ),
        _size( size )
    { }*/

    FixArray( FixArray &&other ) : _data( std::move( other._data ) ), _size( other._size ) {
        other._size = 0;
        other._data = nullptr;
    }

    FixArray( const FixArray &other ) :
        _data( other._size ? new T[ other._size ] : nullptr ),
        _size( other._size )
    {
        std::copy( other.begin(), other.end(), begin() );
    }

    FixArray( std::initializer_list< T > init ) :
        _data( init.size() ? new T[ init.size() ] : nullptr ),
        _size( init.size() )
    {
        std::move( init.begin(), init.end(), begin() );
    }

    using iterator = T *;
    using const_iterator = const T *;
    using reverse_iterator = std::reverse_iterator< iterator >;
    using const_reverse_iterator = std::reverse_iterator< const_iterator >;

    void swap( FixArray &other ) {
        std::swap( _data, other._data );
        std::swap( _size, other._size );
    }

    FixArray &operator=( FixArray other ) {
        swap( other );
        return *this;
    }

    void resize( ssize_t newsize ) {
        std::unique_ptr< T[] > ndata( new T[ newsize ] );
        ssize_t copy = std::min( newsize, _size );
        std::move( _data.get(), _data.get() + copy, ndata.get() );
        _data = std::move( ndata );
        _size = newsize;
    }

    T &operator[]( ssize_t i ) { return _data[ i ]; }
    const T &operator[]( ssize_t i ) const { return _data[ i ]; }

    iterator begin() { return _data.get(); }
    iterator end() { return _data.get() + _size; }

    const_iterator begin() const { return _data.get(); }
    const_iterator cbegin() const { return _data.get(); }
    const_iterator end() const { return _data.get() + _size; }
    const_iterator cend() const { return _data.get() + _size; }

    reverse_iterator rbegin() { return reverse_iterator( end() ); }
    reverse_iterator rend() { return reverse_iterator( begin() ); }

    const_reverse_iterator rbegin() const { return const_reverse_iterator( cend() ); }
    const_reverse_iterator rcbegin() const { return const_reverse_iterator( cend() ); }
    const_reverse_iterator rend() const { return const_reverse_iterator( cbegin() ); }
    const_reverse_iterator rcend() const { return const_reverse_iterator( cbegin() ); }

    T &front() { return *begin(); }
    T &back() { return *rbegin(); }

    const T &front() const { return *begin(); }
    const T &back() const { return *rbegin(); }

    bool operator==( const FixArray &o ) const {
        bool ret = _size == o._size;
        for ( auto ia = begin(), ib = o.begin(); ret && ia != end(); ++ia, ++ib )
            ret &= *ia == *ib;
        return ret;
    }

    bool operator<=( const FixArray &o ) const {
        return std::lexicographical_compare( begin(), end(), o.begin(), o.end(),
                []( const T &a, const T &b ) { return a <= b; } );
    }

    ssize_t size() const { return _size; }
    T *data() { return _data.get(); }
    const T *data() const { return _data.get(); }

  private:
    std::unique_ptr< T[] > _data;
    ssize_t _size;
};

template< typename T >
FixArray< T > appendArray( FixArray< T > arr, T val ) {
    arr.resize( arr.size() + 1 );
    arr.back() = std::move( val );
    return arr;
}

}

namespace std {

template< typename T >
void swap( wibble::FixArray< T > &a, wibble::FixArray< T > &b ) {
    a.swap( b );
}

}

#endif // WIBBLE_FIXARRAY
