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

#ifdef POSIX // hm
        alarm( 1 );
#endif

        for ( int i = 0; i != 4; ++i ) {
            for ( auto &w : threads )
                w.start();
            for ( auto &w : threads )
                w.join();
            assert_eq( sh.counter.load(), 0 );
        }
    }

    struct CounterWorker : wibble::sys::Thread {
        divine::StartDetector detector;
        divine::ApproximateCounter counter;
        std::atomic< int > &queue;
        std::atomic< bool > &interrupted;
        int produce;
        int consume;
        bool terminateEarly;

        template< typename D, typename C >
        CounterWorker( D &d, C &c, std::atomic< int > &q, std::atomic< bool > &i, bool t ) :
            detector( d ),
            counter( c ),
            queue( q ),
            interrupted( i ),
            produce( 0 ),
            consume( 0 ),
            terminateEarly( t )
        {}

        void *main() {
            detector.waitForAll( peers );
            while ( !counter.isZero() ) {
                assert_leq( 0, queue.load() );
                if ( queue == 0 ) {
                    counter.sync();
                    continue;
                }
                if ( produce ) {
                    --produce;
                    ++queue;
                    ++counter;
                }
                if ( consume ) {
                    int v = queue;
                    if ( v == 0 || !queue.compare_exchange_strong( v, v - 1 ) )
                        continue;
                    --consume;
                    --counter;
                }
                // last worker will always fulfill this condition
                // after exactly one consuming operation
                if ( terminateEarly && consume == peers ) {
                    queue = 0;
                    interrupted = true;
                    counter.reset();
                }
                if ( interrupted )
                    break;
            }
            return nullptr;
        }
    };

    void process( bool terminateEarly ) {
        divine::StartDetector::Shared detectorShared;
        divine::ApproximateCounter::Shared counterShared;
        std::atomic< bool > interrupted( false );

        // queueInitials
        std::atomic< int > queue{ 1 };
        divine::ApproximateCounter c( counterShared );
        ++c;
        c.sync();

        std::vector< CounterWorker > threads{ peers,
            CounterWorker{ detectorShared, counterShared, queue, interrupted, terminateEarly } };

        // set consume and produce limits to each worker
        int i = 1;
        for ( auto &w : threads ) {
            w.produce = i;
            // let last worker consume the rest of produced values
            w.consume = peers - i;
            if ( w.consume == 0 )
                w.consume = peers + 1;// also initials
            ++i;
        }

#ifdef POSIX // hm
        alarm( 1 );
#endif

        for ( auto &w : threads )
            w.start();
        for ( auto &w : threads )
            w.join();

        if ( !terminateEarly ) {// do not check it on early termination
            assert_eq( queue.load(), 0 );
            assert_eq( counterShared.counter.load(), 0 );
        }
    }

    Test approximateCounterProcessAll() {
        process( false );
    };

    Test approximateCounterTerminateEarly() {// this test must finish
        process( true );
    }
};

