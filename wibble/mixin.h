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
    bool operator!=( const Self &o ) const { return not comparableSelf() == o; }
    const Self &comparableSelf() const { return *static_cast< const Self * >( this ); }
};

template< typename Self >
struct Comparable : EqualityComparable< Self > {
    bool operator>( const Self &o ) const { return not ( comparableSelf() < o ||
                                                         comparableSelf() == o ); }
    bool operator==( const Self &o ) const { return not ( comparableSelf() < o ||
                                                          o < comparableSelf() ); }
};


}
}

#endif
