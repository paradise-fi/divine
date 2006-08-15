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

/**
 * Mixin with output iterator paperwork.
 *
 * To make an output iterator, one just needs to inherit from this template and
 * implement Self& operator=(const WhaToAccept&)
 */
template< typename Self >
//struct OutputIterator : public std::output_iterator {
struct OutputIterator {
    Self& operator++() { return *static_cast<Self*>(this); }
    Self operator++(int)
    {
   	Self res = *static_cast<Self*>(this);
	++*this;
	return res;
    }
    Self& operator*() { return *static_cast<Self*>(this); }
};

}
}

#endif
