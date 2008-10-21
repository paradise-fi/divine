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
};
