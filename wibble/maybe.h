// -*- C++ -*-

#include <wibble/mixin.h>
#include <wibble/test.h>

#ifndef WIBBLE_MAYBE_H
#define WIBBLE_MAYBE_H

namespace wibble {

/*
  A Maybe type. Values of type Maybe< T > can be either Just T or
  Nothing.

  Maybe< int > foo;
  foo = Maybe::Nothing();
  // or
  foo = Maybe::Just( 5 );
  if ( !foo.nothing() ) {
    int real = foo;
  } else {
    // we haven't got anythig in foo
  }

  Maybe takes a default value, which is normally T(). That is what you
  get if you try to use Nothing as T.
*/

#if __cplusplus < 201103L

template <typename T>
struct Maybe : mixin::Comparable< Maybe< T > > {
    bool nothing() const { return m_nothing; }
    bool isNothing() const { return nothing(); }
    T &value() { return m_value; }
    const T &value() const { return m_value; }
    Maybe( bool n, const T &v ) : m_nothing( n ), m_value( v ) {}
    Maybe( const T &df = T() )
       : m_nothing( true ), m_value( df ) {}
    static Maybe Just( const T &t ) { return Maybe( false, t ); }
    static Maybe Nothing( const T &df = T() ) {
        return Maybe( true, df ); }
    operator T() const { return value(); }

    bool operator <=( const Maybe< T > &o ) const {
        if (o.nothing())
            return true;
        if (nothing())
            return false;
        return value() <= o.value();
    }
protected:
    bool m_nothing:1;
    T m_value;
};

#else

template< typename T >
struct StorableRef {
    T _t;
    T &t() { return _t; }
    const T &t() const { return _t; }
    StorableRef( T t ) : _t( t ) {}
};

template< typename T >
struct StorableRef< T & > {
    T *_t;
    T &t() { return *_t; }
    const T &t() const { return *_t; }
    StorableRef( T &t ) : _t( &t ) {}
};

template< typename _T >
struct Maybe : mixin::Comparable< Maybe< _T > >
{
    using T = _T;

    bool isNothing() const { return _nothing; }
    bool isJust() const { return !_nothing; }

    T &value() {
        assert( isJust() );
        return _v.t.t();
    }

    const T &value() const {
        assert( isJust() );
        return _v.t.t();
    }

    static Maybe Just( const T &t ) { return Maybe( t ); }
    static Maybe Nothing() { return Maybe(); }

    Maybe( const Maybe &m ) {
        _nothing = m.isNothing();
        if ( !_nothing )
            _v.t = m._v.t;
    }

    ~Maybe() {
        if ( !_nothing )
            _v.t.~StorableRef< T >();
    }

    bool operator <=( const Maybe< T > &o ) const {
        if (o.isNothing())
            return true;
        if (isNothing())
            return false;
        return value() <= o.value();
    }

protected:

    Maybe( const T &v ) : _v( v ), _nothing( false ) {}
    Maybe() : _nothing( true ) {}
    struct Empty {
        char x[ sizeof( T ) ];
    };

    union V {
        StorableRef< T > t;
        Empty empty;
        V() : empty() {}
        V( const T &t ) : t( t ) {}
        ~V() { } // see dtor of Maybe
    };
    V _v;
    bool _nothing;
};

#endif

template<>
struct Maybe< void > {
    typedef void T;
    static Maybe Just() { return Maybe( false ); }
    static Maybe Nothing() { return Maybe( true ); }
    bool isNothing() { return _nothing; }
    bool isJust() { return !_nothing; }
private:
    Maybe( bool nothing ) : _nothing( nothing ) {}
    bool _nothing;
};

}

#endif
