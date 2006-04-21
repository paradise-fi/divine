/** -*- C++ -*-
    @file wibble/range.h
    @author Peter Rockai <me@mornfall.net>
*/

#include <iostream> // for noise
#include <iterator>
#include <vector>
#include <set>
#include <algorithm>
#include <ext/algorithm>

#include <wibble/iterator.h>
#include <wibble/shared.h>
#include <wibble/exception.h>

#ifndef WIBBLE_RANGE_H
#define WIBBLE_RANGE_H

namespace wibble {

template< typename > struct Range;
template< typename > struct Consumer;

// auxilliary class, used as Range< T >::iterator
// basically same as Range but with post/pre-increment public
template< typename R >
struct RangeIterator : R {
    RangeIterator() {}
    RangeIterator( const R &r ) : R( r ) {}
public:
    RangeIterator &operator++() { R::operator++(); return *this; }
    RangeIterator operator++(int) { return R::operator++(0); }
    bool operator==( const RangeIterator &r ) {
        return R::operator==( r );
    }
};

// interface to be implemented by all range implementations
// refinement of IteratorInterface (see iterator.h)
// see also amorph.h on how the Morph/Amorph classes are designed
template< typename T >
struct RangeInterface : IteratorInterface< T > {
    virtual void setToEnd() = 0;
    virtual ~RangeInterface() {}
};

// base class for range implementations... mixes into a range
// implementation and provides all the common parts
// if you didn't check amorph.h yet, do it *now*
template< typename T, typename Self, typename Interface = RangeInterface< T > >
struct RangeImpl: IteratorImpl< T, Self, Interface >
{
    typedef IteratorImpl< T, Self, Interface > Base;
    typedef RangeIterator< Self > iterator;
    friend struct RangeIterator< Self >;

    iterator begin() const { return iterator( this->self() ); } // STL-style iteration
    iterator end() const { Self e( this->self() ); e.setToEnd(); return iterator( e ); }
    Range< T > sorted() const;

    void output( Consumer< T > t ) const {
        std::copy( begin(), end(), t );
    }

    virtual bool empty() const {
        return begin() == end();
    }

    virtual ~RangeImpl() {}
protected:
    Self &operator++() { return Base::operator++(); }
    Self operator++(int) { return Base::operator++(0); }
};

// the Amorph of Ranges... if you still didn't check amorph.h, go and
// do it... unless you don't care in which case i am not sure why you
// are reading this anyway

/*
  Range< T > (and all its Morphs (implementations) alike) work as an
  iterable, immutable range of items -- in a way like STL
  const_begin(), const_end() pair of iterators. However, Range packs
  these two in a single object, which you can then pass as a single
  argument or return as a value. There are many Range implementations,
  some of them use STL containers (or just a pair of iterators) as
  backing (IteratorRange, BackedRange), some use other ranges.

  The latter are slightly more interesting, since they provide an
  "adapted" view on the underlying range(s). See FilteredRange,
  TransformedRange, UniqueRange, CastedRange , IntersectionRange.

  GeneratedRange has no "real" backing at all, but use a pair of
  functors and "generates" (by calling those functors) the complete
  range as it is iterated.

  Example usage:

  // create a range from a pair of STL iterators
  Range< int > i = range( myIntVector.begin(), myIntVector.end() );
  // create a range that is filtered view of another range
  Range< int > j = filteredRange( i, predicate );
  std::vector< int > vec;
  // copy out items in j into vec; see also consumer.h
  j.output( consumer( vec ) );
  // vec now contains all items from i (and thus myIntVector) that
  // match 'predicate'

  You don't have to use Range< int > all the time, since it's fairly
  inefficient (adding level of indirection). However in common cases
  it is ok. In the uncommon cases you can use the specific
  implementation type and there you won't suffer the indirection
  penalty. You can also write generic functions that have type of
  range as their template parameter and these will work more
  efficiently if Morph is used directly and less efficiently when the
  Amorph Range is used instead.
 */
template< typename T >
struct Range : Amorph< Range< T >, RangeInterface< T >, 0 >,
               RangeImpl< T, Range< T > >
{
    typedef Amorph< Range< T >, RangeInterface< T >, 0 > Super;
    typedef T ElementType;

    Range( const RangeInterface< T > &i ) : Super( i ) {}
    Range() {}

    T current() const { return this->implInterface()->current(); }
    virtual void advance() { this->implInterface()->advance(); }
    virtual void setToEnd() { this->implInterface()->setToEnd(); }
    virtual bool empty() const { return !this->implInterface() || this->begin() == this->end(); }

    template< typename C > operator Range< C >();
};

template< typename R >
Range< typename R::ElementType > range( R r ) {
    return r;
}

}

// ----- individual range implementations follow
#include <wibble/consumer.h>

namespace wibble {

// a simple pair of iterators -- suffers the same invalidation
// semantics as normal STL iterators and also same problems when the
// backing storage goes out of scope

// this is what you get when using range( iterator1, iterator2 )
// see also range()
template< typename It >
struct IteratorRange : public RangeImpl<
    typename std::iterator_traits< It >::value_type,
    IteratorRange< It > >
{
    typedef typename std::iterator_traits< It >::value_type Value;

    IteratorRange( It c, It e )
        : m_current( c ), m_end( e ) {}

    virtual Value current() const { return *m_current; }
    virtual void advance() { ++m_current; }

    bool operator==( const IteratorRange &r ) const {
        return r.m_current == m_current && r.m_end == m_end;
    }

    void setToEnd() {
        m_current = m_end;
    }

protected:
    It m_current, m_end;
};

// a slightly less simple pair of iterators, this one also holds a
// reference to the container used, so if the container is
// heap-allocated with GC enabled, it won't disappear

// this is what you get when using range( container, ... )

template< typename C >
struct BackedRange : public RangeImpl< typename C::value_type, BackedRange< C > >
{
    typedef typename C::const_iterator It;
public:
    typedef typename C::value_type value_type;
    BackedRange( const C &c, It b, It e ) : m_backing( c ), m_begin( b ), m_end( e ) {}

    bool operator==( const BackedRange &r ) const {
        return r.m_begin == m_begin && r.m_end == m_end;
    }

    void setToEnd() {
        m_begin = m_end;
    }

    virtual value_type current() const { return *m_begin; }
    virtual void advance() { ++m_begin; }

protected:
    const C &m_backing;
    It m_begin, m_end;
};

// first in the series of ranges that use another range as backing
// this one just does static_cast to specified type on all the
// returned elements

// this is what you get when casting Range< S > to Range< T > and S is
// static_cast-able to T

template< typename T, typename Casted >
struct CastedRange : public RangeImpl< T, CastedRange< T, Casted > >
{
    CastedRange( Range< Casted > r ) : m_casted( r ) {}
    virtual T current() const {
        return static_cast< T >( m_casted.current() );
    }
    virtual void advance() { m_casted.advance(); }

    bool operator==( const CastedRange &r ) const {
        return m_casted == r.m_casted;
    }

    void setToEnd() {
        m_casted = m_casted.end();
    }

protected:
    Range< Casted > m_casted;
};

// explicit range-cast... probably not very useful explicitly, just
// use the casting operator of Range
template< typename T, typename C >
Range< T > castedRange( C r ) {
    return CastedRange< T, typename C::ElementType >( r );
}

// old-code-compat for castedRange... slightly silly
template< typename T, typename C >
Range< T > upcastRange( C r ) {
    return CastedRange< T, typename C::ElementType >( r );
}

// the range-cast operator, see castedRange and CastedRange
template< typename T> template< typename C >
Range< T >::operator Range< C >() {
    return castedRange< C >( *this );
}

// range( iterator1, iterator2 ) -- see IteratorRange
template< typename In >
Range< typename In::value_type > range( In b, In e ) {
    return IteratorRange< In >( b, e );
}

// range( container, [iterator1, [iterator2]] ) -- see BackedRange
template< typename C >
Range< typename C::value_type > range( const C &c ) {
    return BackedRange< C >( c, c.begin(), c.end() );
}
template< typename C >
Range< typename C::value_type > range( const C &c,
                                       typename C::const_iterator b ) {
    return BackedRange< C >( c, b, c.end() );
}

template< typename C >
Range< typename C::value_type > range( const C &c,
                                       typename C::const_iterator b,
                                       typename C::const_iterator e ) {
    return BackedRange< C >( c, b, e );
}

template< typename T >
struct IntersectionRange : RangeImpl< T, IntersectionRange< T > >
{
    IntersectionRange() {}
    IntersectionRange( Range< T > r1, Range< T > r2 )
        : m_first( r1 ), m_second( r2 ),
        m_valid( false )
    {
    }

    void find() const {
        if (!m_valid) {
            while ( m_first != m_first.end()
                    && m_second != m_second.end() ) {
                if ( *m_first < *m_second )
                    m_first.advance();
                else if ( *m_second < *m_first )
                    m_second.advance();
                else break; // equal
            }
            if ( m_second.empty() ) m_first.setToEnd();
            else if ( m_first.empty() ) m_second.setToEnd();
        }
        m_valid = true;
    }

    virtual void advance() {
        find();
        m_first.advance();
        m_second.advance();
        m_valid = false;
    }

    virtual T current() const {
        find();
        return m_first.current();
    }

    void setToEnd() {
        m_first = m_first.end();
        m_second = m_second.end();
    }

    bool operator==( const IntersectionRange &f ) const {
        find();
        f.find();
        return m_first == f.m_first;
        // return m_pred == f.m_pred && m_range == f.m_range;
    }

protected:
    mutable Range< T > m_first, m_second;
    mutable bool m_valid:1;
};

template< typename R >
IntersectionRange< typename R::ElementType > intersectionRange( R r1, R r2 ) {
    return IntersectionRange< typename R::ElementType >( r1, r2 );
}

template< typename R, typename Pred >
struct FilteredRange : RangeImpl< typename R::ElementType,
                                  FilteredRange< R, Pred > >
{
    typedef typename R::ElementType ElementType;
    FilteredRange( const R &r, Pred p ) : m_range( r ), m_pred( p ),
        m_valid( false ) {}

    void find() const {
        if (!m_valid)
            m_range = std::find_if( m_range.begin(), m_range.end(), m_pred );
        m_valid = true;
    }

    virtual void advance() {
        find();
        m_range.advance();
        m_valid = false;
    }

    virtual ElementType current() const {
        find();
        return m_range.current();
    }

    void setToEnd() {
        m_range = m_range.end();
    }

    bool operator==( const FilteredRange &f ) const {
        find();
        f.find();
        return m_range == f.m_range;
        // return m_pred == f.m_pred && m_range == f.m_range;
    }

protected:
    mutable R m_range;
    Pred m_pred;
    mutable bool m_valid:1;
};

template< typename R, typename Pred >
FilteredRange< R, Pred > filteredRange(
    R r, Pred p ) {
    return FilteredRange< R, Pred >( r, p );
}

template< typename T >
struct UniqueRange : RangeImpl< T, UniqueRange< T > >
{
    UniqueRange() {}
    UniqueRange( Range< T > r ) : m_range( r ), m_valid( false ) {}

    void find() const {
        if (!m_valid)
            while ( m_range != m_range.end()
                    && m_range.next() != m_range.end()
                    && *m_range == *m_range.next() )
                m_range = m_range.next();
        m_valid = true;
    }

    virtual void advance() {
        find();
        m_range.advance();
        m_valid = false;
    }

    virtual T current() const {
        find();
        return m_range.current();
    }

    void setToEnd() {
        m_range = m_range.end();
    }

    bool operator==( const UniqueRange &r ) const {
        find();
        r.find();
        return m_range == r.m_range;
    }

protected:
    mutable Range< T > m_range;
    mutable bool m_valid:1;
};

template< typename R >
UniqueRange< typename R::ElementType > uniqueRange( R r1 ) {
    return UniqueRange< typename R::ElementType >( r1 );
}

template< typename Transform >
struct TransformedRange : RangeImpl< typename Transform::result_type,
                                     TransformedRange< Transform > >
{
    typedef typename Transform::argument_type Source;
    typedef typename Transform::result_type Result;
    TransformedRange( Range< Source > r, Transform t )
        : m_range( r ), m_transform( t ) {}

    bool operator==( const TransformedRange &o ) const {
        return m_range == o.m_range; // XXX buaaaahaaa!
    }

    Result current() const {
        return m_transform( m_range.current() );
    }

    void advance() {
        m_range.advance();
    }

    void setToEnd() {
        m_range = m_range.end();
    }

protected:
    Range< Source > m_range;
    Transform m_transform;
};

template< typename Trans >
TransformedRange< Trans > transformedRange(
    Range< typename Trans::argument_type > r, Trans t ) {
    return TransformedRange< Trans >( r, t );
}

template< typename T, typename S, typename I >
Range< T > RangeImpl< T, S, I >::sorted() const
{
    std::vector< T > &backing = *new (GC) std::vector< T >();
    output( consumer( backing ) );
    std::sort( backing.begin(), backing.end() );
    return range( backing );
}

template< typename T, typename _Advance, typename _End >
struct GeneratedRange : RangeImpl< T, GeneratedRange< T, _Advance, _End > >
{
    typedef _Advance Advance;
    typedef _End End;

    GeneratedRange() {}
    GeneratedRange( const T &t, const Advance &a, const End &e )
        : m_current( t ), m_advance( a ), m_endPred( e ), m_end( false )
    {
    }

    void advance() {
        m_advance( m_current );
    }

    void setToEnd() {
        m_end = true;
    }

    T current() const { return m_current; }

    bool isEnd() const { return m_end || m_endPred( m_current ); }

    bool operator==( const GeneratedRange &r ) const {
        if ( isEnd() && r.isEnd() ) return true;
        if ( isEnd() || r.isEnd() ) return false;
        return m_current == r.m_current;
    }

protected:
    T m_current;
    Advance m_advance;
    End m_endPred;
    bool m_end;
};

template< typename T, typename A, typename E >
GeneratedRange< T, A, E > generatedRange( T t, A a, E e )
{
    return GeneratedRange< T, A, E >( t, a, e );
}

}

#endif
