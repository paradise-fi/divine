// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <divine/toolkit/parallel.h>
#include <divine/toolkit/pool.h>

using namespace divine;

struct TestPool {

    struct Checker : brick::shmem::Thread
    {
        char padding[128];
        divine::Pool m_pool;
        std::deque< Blob > ptrs;
        int limit;
        unsigned seedp;
        int terminate;
        char padding2[128];

        Pool &pool() {
            return m_pool;
        }
        
        bool decide( int i ) {
            int j = rand() % limit;
            if ( i + j > limit )
                return false;
            return true;
        }

        void main()
        {
            limit = 32*1024;
            int state = 0;
            for ( int i = 0; i < limit; ++i ) {
                assert( state >= 0 );
                if ( decide( i ) || ptrs.empty() ) {
                    ++ state;
                    ptrs.push_back( pool().allocate( 32 ) );
                } else {
                    -- state;
                    pool().free( ptrs.front() );
                    ptrs.pop_front();
                }
            }
            while ( !ptrs.empty() ) {
                pool().free( ptrs.front() );
                ptrs.pop_front();
            }
        }

        Checker()
            : terminate( 0 ) {}
    };

    Test stress() {
        std::vector< Checker > c;
        c.resize( 3 );
        for ( int j = 0; j < 5; ++j ) {
            for ( auto &t : c ) t.start();
            for ( auto &t : c ) t.join();
        }
    }
};
