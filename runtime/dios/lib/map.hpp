// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#ifndef __DIOS_MAP_HPP__
#define __DIOS_MAP_HPP__

#include <sys/divm.h>
#include <brick-data>

namespace __dios {

template < typename T >
struct Array {
    Array() : _data( nullptr ) {}

    using size_type = int;
    using value_type = T;
    using iterator = T *;
    using const_iterator = const T *;

    iterator begin() { return _data; }
    const_iterator cbegin() const { return _data; }
    iterator end() { return _data + size(); }
    const_iterator cend() const { return _data + size(); }
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

template < typename Key, typename Val >
using ArrayMap = brick::data::ArrayMap< Key, Val, std::less< Key >,
    Array< std::pair< Key, Val > > >;

template < typename Key, typename Val >
struct AutoIncMap: public ArrayMap< Key, Val> {
    AutoIncMap(): _nextVal( 0 ) {}

    Val push( const Key& k ) {
        Val v = _nextVal++;
        this->emplace( k, v );
        return v;
    }

    Val _nextVal;
};


} // namespace __dios

# endif // __DIOS_MAP_HPP__