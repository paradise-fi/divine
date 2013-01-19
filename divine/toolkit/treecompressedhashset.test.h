#include <divine/toolkit/hashset.h>
#include <divine/toolkit/treecompressedhashset.h>
#include <divine/toolkit/blob.h>
#include <divine/algorithm/common.h> // hasher
#include <divine/toolkit/pool.h>
#include <random>
#include <cstdint>
#include <algorithm>

using namespace divine;
using divine::algorithm::Hasher;

struct TestTreeCompressedHashSet {

    static std::mt19937 random;

    Test basic() {
        Basic< 0, 68, 16, 512, 4, 32, 10, true >()();
        assert( TestTreeCompressedHashSet::random() );
    }

    // Int -> Int -> Int -> Int -> Int -> Int -> Int -> ()
    template < int slackMin, int slackMax, int sizeMin, int sizeMax,
             int chunkMin, int chunkMax, int tests, bool suffle >
    struct Basic { inline void operator ()() {
        Basic_1b< slackMin, slackMax, sizeMin, sizeMax, chunkMin, chunkMax,
            tests, suffle, chunkMin <= chunkMax >()();
    }};

    // Int -> Int -> Int -> Int -> Int -> Int -> Int -> Bool -> ()
    template < int slackMin, int slackMax, int sizeMin, int sizeMax,
             int chunkMin, int chunkMax, int tests, bool suffle, bool _ff >
    struct Basic_1b { inline void operator ()() { }};

    // Int -> Int -> Int -> Int -> Int -> Int -> Int -> Bool -> ()
    template < int slackMin, int slackMax, int sizeMin, int sizeMax,
             int chunkMin, int chunkMax, int tests, bool suffle >
    struct Basic_1b< slackMin, slackMax, sizeMin, sizeMax, chunkMin,
        chunkMax, tests, suffle, true >
    { inline void operator ()() {
        Basic_2< slackMin, slackMax, sizeMin, sizeMax, chunkMin, tests, suffle >()();
        Basic< slackMin, slackMax, sizeMin, sizeMin, chunkMin * 2, chunkMax,
            tests, suffle >()();
    }};

    // Int -> Int -> Int -> Int -> Int -> Int -> ()
    template < int slackMin, int slackMax, int sizeMin, int sizeMax,
             int chunk, int tests, bool suffle >
    struct Basic_2 { inline void operator ()() {
        Basic_2b< slackMin, slackMax, sizeMin, sizeMax, chunk, tests,
            suffle, sizeMin <= sizeMax>()();
    }};

    // Int -> Int -> Int -> Int -> Int -> Int -> Bool -> ()
    template < int slackMin, int slackMax, int sizeMin, int sizeMax,
             int chunk, int tests, bool suffle, bool _ff >
    struct Basic_2b { inline void operator ()() { }};

    // Int -> Int -> Int -> Int -> Int -> Int -> Bool -> ()
    template < int slackMin, int slackMax, int sizeMin, int sizeMax,
             int chunk, int tests, bool suffle >
    struct Basic_2b< slackMin, slackMax, sizeMin, sizeMax, chunk, tests, suffle, true > { inline void operator ()() {
        Basic_3< slackMin, slackMax, sizeMin, chunk, tests, suffle >()();
        Basic_3< slackMin, slackMax, sizeMin + 1, chunk, tests, suffle >()();
        Basic_2< slackMin, slackMax, sizeMin + 4, sizeMax, chunk, tests, suffle >()();
    }};

    // Int -> Int -> Int -> Int -> Int -> Int -> ()
    template < int slackMin, int slackMax, int size, int chunk, int tests, bool suffle >
    struct Basic_3 { inline void operator ()() {
        Basic_3b< slackMin, slackMax, size, chunk, tests, suffle,
            slackMin <= slackMax >()();
    }};

    // Int -> Int -> Int -> Int -> Int -> Int -> Bool -> ()
    template < int slackMin, int slackMax, int size, int chunk, int tests,
             bool suffle, bool _ff >
    struct Basic_3b { inline void operator ()() { }};

    // Int -> Int -> Int -> Int -> Int -> Int -> Bool -> ()
    template < int slackMin, int slackMax, int size, int chunk, int tests,
             bool suffle >
    struct Basic_3b< slackMin, slackMax, size, chunk, tests, suffle, true > {
        inline void operator ()() {
            Basic_4< slackMin, size, chunk, tests, suffle >()();
            Basic_3< slackMin + 4, slackMax, size, chunk, tests, suffle >()();
        }
    };

    template < int slack, int size, int chunk, int tests, bool suffle >
    struct Basic_4 { inline void operator ()() {
        for ( int i = 0; i < tests; ++i )
            Basic_5< slack, size, chunk, suffle >()();
    }};

    template < int slack, int size, int chunk, bool suffle >
    struct Basic_5 {
        inline void operator ()() {
            Pool p;
            Hasher h( p, slack );
            TreeCompressedHashSet< HashSet, Blob, Hasher, chunk > set( h );

            std::deque< Blob > blobs;
            for ( int i = 0; i < 1000; ++i ) {
                Blob b = randBlob( p );
                blobs.push_back( b );
                set.insert( b );
            }

            int i = 0;
            if ( suffle )
                std::shuffle( blobs.begin(), blobs.end(),
                        TestTreeCompressedHashSet::random );
            for ( Blob b : blobs ) {
                ++i;
                Blob b0 = set.get( b );
                assert( h.valid( b0 ) );
                assert( p.compare( b, b0, 0, slack ) == 0 );
                Blob b1 = set.getFull( b );
                assert( h.equal( b, b1 ) );
                assert( p.compare( b, b1, 0, slack ) == 0 );
                p.free( b1 );
            }

            if ( suffle )
                std::shuffle( blobs.begin(), blobs.end(),
                        TestTreeCompressedHashSet::random );

            while ( !blobs.empty() ) {
                ++i;
                Blob b = blobs.front();
                blobs.pop_front();
                assert( p.compare( b, set.get( b ), 0, slack ) == 0 );
                Blob b1 = set.getFull( b );
                assert( h.equal( b, b1 ) );
                assert( p.compare( b, b1, 0, slack ) == 0 );
                p.free( b1 );
                p.free( b );
            }

            set.freeAll();
        }

        Blob randBlob( Pool& p ) {
            int fullSize = slack + size;
            Blob b( p, fullSize );
            for ( int i = 0; i < fullSize; i += 4 )
                p.template get< std::uint_fast32_t >( b, i ) =
                    TestTreeCompressedHashSet::random();
            return b;
        }
    };
};

std::mt19937 TestTreeCompressedHashSet::random = std::mt19937( 0 );

int main() {
    TestTreeCompressedHashSet tt;
    tt.basic();
    return 0;
}
