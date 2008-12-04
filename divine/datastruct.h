// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>
#include <wibble/test.h> // assert

#ifndef DIVINE_DATASTRUCT_H
#define DIVINE_DATASTRUCT_H

namespace divine {

template< typename T, int size >
struct Circular {
    T items[ size ];
    int _first, _count;

    T *start() {
        return items;
    }

    T *first() {
        return items + _first;
    }

    T *last() {
        return items + (_first + _count - 1) % size;
    }

    void add( T t ) {
        assert( _count < size );
        ++ _count;
        *last() = t;
    }

    bool full() {
        return _count == size;
    }

    void drop( int n ) {
        assert( n <= _count );
        _first += n;
        _count -= n;
        _first %= size;
    }

    Circular() : _count( 0 ), _first( 0 ) {}
};

}

#endif
