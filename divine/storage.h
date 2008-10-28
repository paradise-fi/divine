// -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net>

#include <wibble/sfinae.h>

#include <divine/hashmap.h>
#include <divine/stateallocator.h>

#ifndef DIVINE_STORAGE_H
#define DIVINE_STORAGE_H

namespace divine {
namespace storage {

namespace impl {

using namespace wibble;

template< typename Bundle, typename Self >
struct Partition
{
    typedef Unit IsStorage;

    typedef typename Bundle::State State;
    typedef HashMap< State, Unit > Table;
    typedef typename Table::Reference Reference;

    Self &self() { return *static_cast< Self* >( this ); }

    Table m_table;

    divine::StateAllocator *newAllocator() {
        return new DefaultAllocator< State >();
    }

    void deallocate( State st ) {
        delete[] st.pointer();
    }

    Reference insert( State st ) {
        return self().table().insert( st );
    }

    Reference get( State st ) {
        return self().table().get( st );
    }

    Table &table() { return m_table; }

    Partition( typename Bundle::Controller &c )
        : m_table( c.config().storageInitial(), c.config().storageFactor() )
    {}
};

template< typename Bundle, typename Self >
struct Shadow : Partition< Bundle, Self >
{
    typedef typename Bundle::State State;
    typedef HashMap< State, Unit > Table;
    Table *m_tablePtr;
    Table &table() { return m_tablePtr ? *m_tablePtr : this->m_table; }

    void setTable( Table &t ) { m_tablePtr = &t; }

    Shadow( typename Bundle::Controller &c )
        : Partition< Bundle, Self >( c )
    {
        m_tablePtr = 0;
    }
};

template< typename Bundle, typename Self >
struct PooledPartition : Partition< Bundle, Self >
{
    typedef Unit IsStorage;
    typedef Unit IsStealing;

    typedef typename Bundle::State State;
    typedef HashMap< State, Unit > Table;
    typedef typename Table::Reference Reference;

    Pool m_pool;

    divine::StateAllocator *newAllocator() {
        return new PooledAllocator< State >( m_pool );
    }

    void deallocate( typename Bundle::State st ) {
        m_pool.free( st.pointer(), State::allocationSize( st.size() ) );
    }

    void steal( typename Bundle::State st ) {
        m_pool.steal( st.pointer(), State::allocationSize( st.size() ) );
    }

    Pool &pool() { return m_pool; }

    PooledPartition( typename Bundle::Controller &c )
        : Partition< Bundle, Self >( c )
    {}
};

}

typedef Finalize< impl::Partition > Partition;
typedef Finalize< impl::PooledPartition > PooledPartition;
typedef Finalize< impl::Shadow > Shadow;

template< typename T, typename Enable = Unit >
struct IsStealing {
    static const bool value = false;
};

template< typename T >
struct IsStealing <
    T, typename T::template Get< VoidBundle >::IsStealing >
{
    static const bool value = true;
};

template< typename T >
struct IsStealing< T, typename T::IsStealing >
{
    static const bool value = true;
};

}
}

template< typename A, typename B >
struct SimpleTAnd {};

namespace std {

template< typename A, typename B >
void swap( divine::storage::Partition::Get< A > &a,
           divine::storage::Partition::Get< B > &b )
{
    swap( a.table(), b.table() );
}

template< typename A, typename B >
void swap( divine::storage::PooledPartition::Get< A > &a,
           divine::storage::PooledPartition::Get< B > &b )
{
    swap( a.table(), b.table() );
    swap( a.pool(), b.pool() );
}

template< typename A, typename B >
SimpleTAnd< typename A::IsStorage, typename B::IsStorage > swap( A &a, B &b )
{
    // only swap pointers (!)
    swap( a.m_table, b.m_table );
    return SimpleTAnd< typename A::IsStorage, typename B::IsStorage >();
}

}

#endif
