/** -*- C++ -*-
    @file utils/range.h
    @author Peter Rockai <me@mornfall.net>
*/

#ifndef WIBBLE_MIXIN_H
#define WIBBLE_MIXIN_H

namespace wibble {
namespace mixin {

template< typename Self >
struct EqualityComparable {
    const Self &self() const { return *static_cast< const Self * >( this ); }
    bool operator!=( const Self &o ) const { return not self() == o; }
};

template< typename Self >
struct Comparable : EqualityComparable< Self > {
    const Self &self() const { return *static_cast< const Self * >( this ); }
    bool operator>( const Self &o ) const { return not ( self() < o || self() == o ); }
    bool operator==( const Self &o ) const { return not ( self() < o || o < self() ); }
};


}
}

#endif
