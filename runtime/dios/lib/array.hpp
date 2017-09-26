// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#ifndef __DIOS_ARRAY_HPP__
#define __DIOS_ARRAY_HPP__

#include <sys/divm.h>

namespace __dios {

template < typename T >
struct Array {
    Array() : _data( nullptr ) {}

    using size_type = int;
    using value_type = T;
    using iterator = T *;
    using const_iterator = const T *;
    using reverse_iterator = std::reverse_iterator< iterator >;
    using const_reverse_iterator = std::reverse_iterator< const_iterator >;

    iterator begin() { return _data; }
    const_iterator begin() const { return _data; }
    const_iterator cbegin() const { return _data; }

    iterator end() { return _data + size(); }
    const_iterator end() const { return _data + size(); }
    const_iterator cend() const { return _data + size(); }

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
            _data[ i ].~T();
        __vm_obj_free( _data );
        _data = nullptr;
    }

    void push_back( const T& t ) {
        _resize( size() + 1 );
        new ( &back() ) T( t );
    }

    template < typename... Args >
    void emplace_back( Args&&... args ) {
        _resize( size() + 1 );
        new ( &back() ) T( std::forward< Args >( args )... );
    }

    void pop_back() {
        back().~T();
        _resize( size() - 1 );
    }

    T& operator[]( size_type idx ) { return _data[ idx ]; }
    const T& operator[]( size_type idx ) const { return _data[ idx ]; }

    void _resize( size_type n ) {
        if ( n == 0 ) {
            if ( _data )
                __vm_obj_free( _data );
            _data = nullptr;
        }
        else if ( empty() )
            _data = static_cast< T * >( __vm_obj_make( n * sizeof( T ) ) );
        else
            __vm_obj_resize( _data, n * sizeof( T ) );
    }

    T *_data;
};

} // namespace __dios

# endif // __DIOS_ARRAY_HPP__
