/** -*- C++ -*-
    @file utils/range.h
    @author Peter Rockai <me@mornfall.net>
*/

#ifndef WIBBLE_MIXIN_H
#define WIBBLE_MIXIN_H

namespace wibble {
namespace mixin {

template< typename Self >
struct Comparable {
    const Self &cmpSelf() const { return *static_cast< const Self * >( this ); }
    bool operator!=( const Self &o ) const { return not( cmpSelf() == o ); }
    bool operator==( const Self &o ) const { return cmpSelf() <= o && o <= cmpSelf(); }
    bool operator<( const Self &o ) const { return cmpSelf() <= o && cmpSelf() != o; }
    bool operator>( const Self &o ) const { return o <= cmpSelf() && cmpSelf() != o; }
    bool operator>=( const Self &o ) const { return o <= cmpSelf(); }

    // reimplement this one
    // bool operator<=( const Self &o ) const { return this <= &o; }
};


}
}

#endif
