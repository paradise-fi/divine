#include <divine/toolkit/shmem.h>
#include <wibble/sys/thread.h>
#include <wibble/test.h>

#include <vector>

struct TestSharedMemoryUtilities {

    Test sequential() {
        divine::StartDetector::Shared sh;
        divine::StartDetector detector( sh );

        const unsigned short peers = 15;

        for ( int r = 0; r != 2; ++r ) {
            for ( int i = 0; i < peers; ++i ) {
                detector.visitorStart();
                assert( detector.waitForAll( peers ) );
            }
            assert( !detector.waitForAll( peers ) );
        }
    }

    struct Worker : wibble::sys::Thread {

        divine::StartDetector detector;
        unsigned short peers;

        Worker( divine::StartDetector::Shared &sh, unsigned short peers ) :
            detector( sh ), peers( peers )
        {}

        void* main() {
            detector.visitorStart();
            while ( detector.waitForAll( peers ) );
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
        }
    }
};

