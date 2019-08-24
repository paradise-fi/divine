// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#pragma once
#include <util/array.hpp>
#include <brick-compact>

namespace __dios
{
    template < typename Key, typename Val, int PT = _VM_PT_Heap >
    using ArrayMap = brq::array_map< Key, Val, brq::insert_sort< brq::less_map >,
                                     Array< std::pair< Key, Val >, PT > >;

    template < typename Key, typename Val, int PT = _VM_PT_Heap >
    struct AutoIncMap: public ArrayMap< Key, Val, PT >
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
}
