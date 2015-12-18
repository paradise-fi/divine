// -*- C++ -*-
#include <cstdarg>
#include <cassert>
#include <vector>
#include <sstream>

struct Element {
    virtual void _void() {};
};

struct Serialise : Element {
    virtual int size() = 0;
    virtual void write( char *where ) = 0;
    virtual std::string print() { return std::string(); }
};

// A building element of a circuit that provides an output value.
template< typename T >
struct Value : virtual Serialise {
    virtual T get() = 0;

    virtual int size() { return sizeof( T ); }
    virtual void write( char *where ) {
        *(T *)where = get();
    }

    std::string print() {
        std::stringstream s;
        s << get();
        return s.str();
    }
};

// An input *port*, for use within building blocks.
template< typename T >
struct In : Element {
    Value< T > *p;
    Value< T > *operator->() { return p; }
    In() { p = 0; }
};

struct Block : Element {
    typedef std::pair< Element *, Element * > Connection;
    std::vector< Connection > conns;

    // Connect a value-providing block to an input port.
    template< typename T >
    void connect( Value< T > &from, Element &parent, In< T > &to )
    {
        to.p = &from;
        conns.push_back( std::make_pair( &from, &parent ) );
    }

    // Connect a value-providing block to an input port.
    template< typename S, typename T >
    void connect( Value< S > &from, T &parent, int idx )
    {
        parent.in( idx ).p = &from;
        conns.push_back( std::make_pair( &from, &parent ) );
    }
};

template< typename T, int Max >
struct InSet {
    typedef std::vector< In< T > > Set;
    typedef typename Set::iterator iterator;
    Set set;

    In< T > &operator()( int idx ) {
	assert( idx < Max );
        if ( set.size() <= idx )
            set.resize( idx + 1 );
        return set[ idx ];
    }

    iterator begin() { return set.begin(); }
    iterator end() { return set.end(); }
};

// A building element of a circuit that needs to act upon the clock signal.
struct Clock {
    virtual void tick() = 0;
};

// A stateful building element: maintains (serialisable) state and reacts to
// clock ticks.
struct Stateful : virtual Clock, virtual Serialise {
    virtual void read( char *from ) = 0;
};

// This class wraps the complete system and provides a single synchronised
// clock for all the stateful elements.
struct System : Clock
{
    typedef std::vector< Stateful * > Elements;
    Elements elements;
    Block *_circuit;

    template< typename T >
    T *circuit() {
        return static_cast< T * >( _circuit );
    }

    virtual void tick() {
        for ( Elements::iterator i = elements.begin(); i != elements.end(); ++i )
            (*i)->tick();
    }

    System( Block &b ) {
        int swaps;

        /* TODO: There are probably better than cubic top-sort algorithms... */
        for ( int iter = 0; iter < b.conns.size(); ++iter ) {
            for ( int i = 0; i < b.conns.size(); ++i ) {
                for ( int j = 0; j < i; ++j ) {
                    if (b.conns[j].first == b.conns[i].second &&
                        b.conns[j].first != b.conns[j].second) {
                        std::swap(b.conns[j], b.conns[i]);
                        ++ swaps;
                    }
                }
            }
        };

        for ( int i = 0; i < b.conns.size(); ++i ) {
            Stateful *s = dynamic_cast< Stateful * >( b.conns[i].first );
            if ( s )
                elements.push_back( s );
        }

        _circuit = &b;
    }

    int size() {
        int s = 0;
        for ( Elements::iterator i = elements.begin(); i != elements.end(); ++i )
            s += (*i)->size();
        return s;
    }

    void write( char *where ) {
        for ( Elements::iterator i = elements.begin(); i != elements.end(); ++i ) {
            (*i)->write( where );
            where += (*i)->size();
        }
    }

    void read( char *from ) {
        for ( Elements::iterator i = elements.begin(); i != elements.end(); ++i ) {
            (*i)->read( from );
            from += (*i)->size();
        }
    }

    std::string print() {
        std::string s = "[";
        for ( Elements::iterator i = elements.begin(); i != elements.end(); ++i ) {
            s += (*i)->print();
            if ( i + 1 != elements.end() )
                s += ", ";
        }
        s += "]";
        return s;
    }
};

// ------------ example block implementations -------------

namespace example {

// Implementation of the delay block. This is an example of a stateful element.
template< typename T >
struct Delay : virtual Value< T >, virtual Clock, virtual Stateful {
    In< T > in;
    T last;
    virtual T get() { return last; }
    void tick() { last = in->get(); }
    void read( char *from ) {
        last = *(T *)from;
    }
    Delay() : last() {}
};

// An input *block* (not a port).
template< typename T >
struct Input : virtual Value< T >, virtual In< T >, virtual Stateful {
    T value;
    virtual T get() { return value; }
    Input() : value() {}
    Input( T t ) : value( t ) {}
    Input< T > &operator=( T t ) { value = t; return *this; }
    void tick() {}
    void read( char *from ) {
        value = *(T *)from;
    }
};

// A simple output *block*.
template< typename T >
struct Output : Value< T > {
    Value< T > *from;
    virtual T get() { return from->get(); }
};

// A type-cast block.
template< typename From, typename To >
struct Cast : Value< To > {
    In< From > in;

    virtual To get() {
        return static_cast< To >( in->get() );
    }
};

template< typename T >
struct Sum : Value< T > {
    InSet< T, 4 > in;

    virtual T get() {
        T acc = 0;
        for ( typename InSet< T, 4 >::iterator i = in.begin(); i != in.end(); ++i )
            acc += (*i)->get();
        return acc;
    }
};

template< typename T >
struct Product : Value< T > {
    In< T > a, b;

    virtual T get() {
        return a->get() * b->get();
    }
};

template< typename T >
struct Min : Value< T > {
    In< T > a, b;

    virtual T get() {
        return std::min( a->get(), b->get() );
    }
};

template< typename T >
struct Gain : Value< T > {
    In< T > in;
    T value;

    virtual T get() {
        return value * in->get();
    }

    Gain( T v ) : value( v ) {}
};

template< typename T >
struct MinMax : Value< T > {
    In< T > a, b;
    enum Function { Min, Max };
    Function f;

    MinMax( Function _f ) : f( _f ) {}
    T get() {
        switch (f) {
            case Min: return std::min( a->get(), b->get() );
            case Max: return std::max( a->get(), b->get() );
        }
    }
};

}

