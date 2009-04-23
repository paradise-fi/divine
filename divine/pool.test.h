// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <divine/parallel.h>
#include <divine/pool.h>

using namespace divine;

struct TestPool {
    Test steal() {
        Pool p;
        char *c = p.allocate( 32 );
        p.steal( c, 32 );
        assert_eq( p.allocate( 32 ), c );
    }

    struct Checker : wibble::sys::Thread
    {
        char padding[128];
        divine::Pool m_pool;
        std::deque< char * > ptrs;
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

        void *main()
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
                    pool().free( ptrs.front(), 32 );
                    ptrs.pop_front();
                }
            }
            while ( !ptrs.empty() ) {
                pool().free( ptrs.front(), 32 );
                ptrs.pop_front();
            }
            return 0;
        }

        Checker()
            : terminate( 0 ) {}
    };

    Test stress() {
        std::vector< Checker > c;
        c.resize( 3 );
        int j = 0;
        for ( int j = 0; j < 5; ++j ) {
            for ( int i = 0; i < c.size(); ++i )
                c[ i ].start();
            for ( int i = 0; i < c.size(); ++i )
                c[ i ].join();
        }
    }
};
