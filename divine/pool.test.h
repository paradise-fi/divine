// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <divine/parallel.h>
#include <divine/pool.h>

using namespace divine;

struct TestPool {
    struct Threads {
        typedef int Shared;
        Shared shared;
        Pool p;
        void check() {
            // std::cerr << "my = " << (void *)&p << ", thread = " << (void *)ThreadPoolManager::get() << std::endl;
            assert_eq( &p, ThreadPoolManager::force( &p ) );
            assert_eq( &p, ThreadPoolManager::get() );
        }
        Threads() { check(); }
    };

    Test threads() {
        Threads m;
        m.check();
        Parallel< Threads > p( m, 10 );
        p.run( &Threads::check );
    }

    Test steal() {
        Pool p;
        char *c = p.alloc( 32 );
        p.steal( c, 32 );
        assert_eq( p.alloc( 32 ), c );
    }

    struct Checker : wibble::sys::Thread
    {
        char padding[128];
        divine::Pool pool;
        std::deque< char * > ptrs;
        int limit;
        unsigned seedp;
        int terminate;
        char padding2[128];
        
        bool decide( int i ) {
            int j = rand() % limit;
            if ( i + j > limit )
                return false;
            return true;
        }

        void *main()
        {
            limit = 1024*1024;
            for ( int i = 0; i < limit; ++i ) {
                if ( decide( i ) || ptrs.empty() ) {
                    ptrs.push_back( pool.alloc( 32 ) );
                } else {
                    pool.free( ptrs.front(), 32 );
                    ptrs.pop_front();
                }
            }
            return 0;
        }

        Checker() : terminate( 0 ) {}
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
