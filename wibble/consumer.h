/** -*- C++ -*-
    @file wibble/consumer.h
    @author Peter Rockai <me@mornfall.net>
*/

#include <iterator>

#include <wibble/amorph.h>
#include <wibble/range.h>

#ifndef WIBBLE_CONSUMER_H
#define WIBBLE_CONSUMER_H

namespace wibble {

template< typename T >
struct ConsumerInterface
{
    typedef T InputType;
    virtual void consume( const T &a ) = 0;
    virtual void consume( Range< T > s ) {
        while ( s != s.end() ) {
            consume( s.current() );
            ++s;
        }
    }
    virtual ~ConsumerInterface() {}
};

template< typename T, typename Self, typename Interface = ConsumerInterface< T > >
struct ConsumerImpl: MorphImpl< Self, Interface >
{
    typedef std::output_iterator_tag iterator_category;

    Consumer< T > &operator++() { return this->self(); }
    Consumer< T > &operator++(int) { return this->self(); }
    Consumer< T > &operator*() { return this->self(); }
    Consumer< T > &operator=( const T &a ) {
        this->self()->consume( a );
        return this->self();
    }
    operator Consumer< T >() const;
};

template< typename T >
struct Consumer: Amorph< Consumer< T >, ConsumerInterface< T > >,
                 ConsumerImpl< T, Consumer< T > >
{
    typedef Amorph< Consumer< T >, ConsumerInterface< T > > Super;

    typedef void value_type;
    typedef void difference_type;
    typedef void pointer;
    typedef void reference;

    Consumer( const typename Super::Interface *i ) : Super( i ) {}
    Consumer( const Consumer &i ) : Super( i ) {}
    Consumer() {}

    virtual void consume( const T &a ) {
        return this->implInterface()->consume( a );
    }

    Consumer< T > &operator=( const T &a ) {
        consume( a );
        return *this;
    }
    // output iterator - can't read or move
};

template< typename T, typename S, typename I >
inline ConsumerImpl< T, S, I >::operator Consumer< T >() const
{
    return Consumer< T >( this );
}

template< typename T, typename Out >
struct ConsumerFromIterator : ConsumerImpl<
    T, ConsumerFromIterator< T, Out > >
{
    ConsumerFromIterator( Out out ) : m_out( out ) {}
    virtual void consume( const T& a ) {
        *m_out = a;
        ++m_out;
    }
protected:
    Out m_out;
};

template< typename T, typename Out >
Consumer< T > consumer( Out out ) {
    return ConsumerFromIterator< T, Out >( out );
}

template< typename T >
Consumer< T > consumer( std::set< T > &c ) {
    return consumer< T >( std::inserter( c, c.begin() ) );
}

/* causes ambiguities: template< typename Out >
Consumer< typename std::iterator_traits< Out >::value_type > consumer( Out out ) {
    return ConsumerFromIterator< typename std::iterator_traits< Out >::value_type, Out >( out );
    } */

template< typename Base >
Consumer< typename Base::InputType > consumer( Base b ) {
    return static_cast<Consumer< typename Base::InputType > >( &b );
}

}

#endif
