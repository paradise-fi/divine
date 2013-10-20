#include <divine/toolkit/shmem.h>
#include <wibble/sys/thread.h>
#include <wibble/test.h>

#include <vector>

struct TestSharedMemoryUtilities {

    struct Worker : wibble::sys::Thread {

        divine::StartDetector detector;
        unsigned short peers;

        Worker( divine::StartDetector::Shared &sh, unsigned short peers ) :
            detector( sh ), peers( peers )
        {}

        void* main() {
            detector.waitForAll( peers );
            return nullptr;
        }
    };

    Test parallel() {// this test must finish
        const unsigned peers = 12;
        divine::StartDetector::Shared sh;
        std::vector< Worker > threads{ peers, Worker{ sh, peers } };

        alarm( 1 );

        for ( int i = 0; i != 4; ++i ) {
            for ( Worker &w : threads )
                w.start();
            for ( Worker &w : threads )
                w.join();
            assert_eq( sh.counter.load(), 0 );
        }
    }
};

