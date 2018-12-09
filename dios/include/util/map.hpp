// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#ifndef __DIOS_MAP_HPP__
#define __DIOS_MAP_HPP__

#include <util/array.hpp>
#include <brick-data>

namespace __dios {

namespace data = brick::data;

template < typename Key, typename Val >
using ArrayMap = data::ArrayMap< Key, Val, data::InsertSort, Array< std::pair< Key, Val > > >;

template < typename Key, typename Val >
struct AutoIncMap: public ArrayMap< Key, Val>
{
    AutoIncMap(): _nextVal( 0 ) {}

    Val push( const Key& k )
    {
        Val v = _nextVal++;
        this->emplace( k, v );
        return v;
    }

    Val _nextVal;
};


} // namespace __dios

# endif // __DIOS_MAP_HPP__
