#include <divine/toolkit/shmem.h>
#include <wibble/sys/thread.h>
#include <wibble/test.h>

#include <vector>

struct TestSharedMemoryUtilities {
    static const int peers = 12;

    struct DetectorWorker : wibble::sys::Thread {

        divine::StartDetector detector;

        DetectorWorker( divine::StartDetector::Shared &sh ) :
            detector( sh )
        {}

        void* main() {
            detector.waitForAll( peers );
            return nullptr;
        }
    };

    Test startDetector() {// this test must finish
        divine::StartDetector::Shared sh;
        std::vector< DetectorWorker > threads{ peers, DetectorWorker{ sh } };

        alarm( 1 );

        for ( int i = 0; i != 4; ++i ) {
            for ( auto &w : threads )
                w.start();
            for ( auto &w : threads )
                w.join();
            assert_eq( sh.counter.load(), 0 );
        }
    }
};

