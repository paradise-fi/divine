// -*- C++ -*- (c) 2008, 2010 Petr Rockai <me@mornfall.net>

#include <wibble/test.h> // assert

#ifndef DIVINE_CIRCULAR_H
#define DIVINE_CIRCULAR_H

namespace divine {

template< typename T, int _size >
struct Circular {
    int __size, // for non-C++ access
        _count, _first;
    T items[ _size ];

    T *start() { return items; }
    T *nth( int i ) { return items + (_first + i) % size(); }
    T *first() { return nth( 0 ); }
    T *last() { return nth( _count - 1 ); }
    T &operator[]( int i ) { return *nth( i ); }

    void add( T t ) {
        assert( _count < size() );
        ++ _count;
        *last() = t;
    }

    int count() { return _count; }
    int space() { return size() - _count; }
    int size() { return __size; }
    bool full() { return _count == size(); }
    bool empty() { return !_count; }
    void clear() { drop( count() ); }

    void drop( int n ) {
        assert( n <= _count );
        _first += n;
        _count -= n;
        _first %= size();
    }

    void unadd( int n ) {
        _count -= n;
    }

    Circular() : __size( _size ), _count( 0 ), _first( 0 ) {}
};

}

#endif
