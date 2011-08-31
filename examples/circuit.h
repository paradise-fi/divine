// -*- C++ -*-
#include <cstdarg>
#include <cassert>
#include <vector>
#include <sstream>

struct Serialise {
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
struct In {
    Value< T > *p;
    Value< T > *operator->() { return p; }
    In() { p = 0; }
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

// Connect a value-providing block to an input port.
template< typename T >
void connect( Value< T > &from, In< T > &to )
{
    to.p = &from;
}

template< typename T >
void connect( Value< T > &from, Output< T > &to )
{
    to.from = &from;
}

// This class wraps the complete system and provides a single synchronised
// clock for all the stateful elements.
struct System : Clock
{
    typedef std::vector< Stateful * > Elements;
    Elements elements;
    void *_circuit;

    template< typename T >
    T *circuit() {
        return static_cast< T * >( _circuit );
    }

    virtual void tick() {
        for ( Elements::iterator i = elements.begin(); i != elements.end(); ++i )
            (*i)->tick();
    }

    System &add( Stateful &e ) {
        elements.push_back( &e );
        return *this;
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

