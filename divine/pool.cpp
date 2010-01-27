// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <divine/pool.h>

namespace divine {

#ifndef DISABLE_POOLS
Pool::Pool() {
    m_groupCount = 0;
    m_groups.reserve( 8096 ); // FIXME
}

Pool::Pool( const Pool& ) {
    m_groupCount = 0;
    m_groups.reserve( 8096 ); // FIXME
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
#endif
