// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <divine/pool.h>
#include <divine/blob.h>

namespace divine {

#ifndef DISABLE_POOLS
Pool::Pool() {
    m_groupStructSize = sizeof( Group );
    m_groups.reserve( 8096 ); // FIXME
    ffi_update();
}

Pool::Pool( const Pool& ) {
    m_groups.reserve( 8096 ); // FIXME
    ffi_update();
}

Pool::~Pool() {
    for (Groups::iterator i = m_groups.begin(); i != m_groups.end(); i++) {
        for (std::vector< Block >::iterator b = i->blocks.begin(); b != i->blocks.end(); b++) {
            delete[] b->start;
        }

        i->blocks.clear();
    }
}
#endif

}

#ifndef DISABLE_POOLS
extern "C" void *pool_extend( void *pool, int sz ) {
    divine::Pool *p = (divine::Pool *) pool;
    if ( !p->group( sz ) )
        p->createGroup( sz );
    p->newBlock( p->group( sz ) );
    return &(p->m_groups.front());
}

extern "C" void *pool_allocate( void *pool, int sz ) {
    divine::Pool *p = (divine::Pool *) pool;
    return p->allocate( sz );
}

extern "C" void *pool_allocate_blob( void *pool, int sz ) {
    divine::Pool *p = (divine::Pool *) pool;
    divine::Blob b( *p, sz );
    return b.pointer();
}
#endif
