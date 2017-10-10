// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#ifndef __DIOS_ARRAY_HPP__
#define __DIOS_ARRAY_HPP__

#include <sys/divm.h>
#include <iterator>
#include <cstring>

namespace __dios {

template < typename T >
struct Array {
    Array() : _data( nullptr ) {}

    ~Array() {
        erase( begin(), end() );
    }

    Array( const Array& other ) : _data( nullptr ) {
        _resize( other.size() );
        for( int i = 0; i != other.size(); i++ ) {
            new ( begin() + i ) T( other[ i ] );
        }
    };

    Array( Array&& other ) : _data( nullptr ) { swap( other ); };

    Array& operator=( Array other ) {
        swap( other );
        return *this;
    };

    void swap( Array& other ) {
        using std::swap;
        swap( _data, other._data );
    }

    using size_type = int;
    using value_type = T;
    using iterator = T *;
    using const_iterator = const T *;
    using reverse_iterator = std::reverse_iterator< iterator >;
    using const_reverse_iterator = std::reverse_iterator< const_iterator >;

    struct _Item {
        T _items[ 0 ];
        T *get() { return reinterpret_cast< T * >( _items ); }
    };

    iterator begin() { return _data->get(); }
    const_iterator begin() const { return _data->get(); }
    const_iterator cbegin() const { return _data->get(); }

    iterator end() { return _data->get() + size(); }
    const_iterator end() const { return _data->get() + size(); }
    const_iterator cend() const { return _data->get() + size(); }

    reverse_iterator rbegin() { return reverse_iterator( end() ); }
    const_reverse_iterator rbegin() const { return reverse_iterator( end() ); }
    const_reverse_iterator crbegin() const { return reverse_iterator( end() ); }

    reverse_iterator rend() { return reverse_iterator( begin() ); }
    const_reverse_iterator rend() const { return const_reverse_iterator( begin() ); }
    const_reverse_iterator crend() const { return const_reverse_iterator( begin() ); }

    T& back() { return *( end() - 1 ); }

    bool empty() const { return !_data; }
    size_type size() const  { return empty() ? 0 : __vm_obj_size( _data ) / sizeof( T ); }
    void clear() {
        if ( empty() )
            return;
        auto s = size();
        for ( size_type i = 0; i != s; i++ )
            _data->get()[ i ].~T();
        __vm_obj_free( _data );
        _data = nullptr;
    }

    void push_back( const T& t ) {
        _resize( size() + 1 );
        new ( &back() ) T( t );
    }

    template < typename... Args >
    T& emplace_back( Args&&... args ) {
        _resize( size() + 1 );
        new ( &back() ) T( std::forward< Args >( args )... );
        return back();
    }

    void pop_back() {
        back().~T();
        _resize( size() - 1 );
    }

    iterator erase( iterator it ) {
        return erase( it, std::next( it ) );
    }

    iterator erase( iterator first, iterator last ) {
        if ( empty() )
            return;
        int origSize = size();
        int shift = std::distance( first, last );
        for ( iterator it = first; it != last; it++ )
            it->~T();
        memmove( first, last, ( end() - last ) * sizeof( T ) );
        _resize( origSize - shift );
        return last;
    }

    T& operator[]( size_type idx ) { return _data->get()[ idx ]; }
    const T& operator[]( size_type idx ) const { return _data->get()[ idx ]; }

    void _resize( size_type n ) {
        if ( n == 0 ) {
            if ( _data )
                __vm_obj_free( _data );
            _data = nullptr;
        }
        else if ( empty() )
            _data = static_cast< _Item * >( __vm_obj_make( n * sizeof( T ) ) );
        else
            __vm_obj_resize( _data, n * sizeof( T ) );
    }

    _Item *_data;
};

} // namespace __dios

# endif // __DIOS_ARRAY_HPP__
