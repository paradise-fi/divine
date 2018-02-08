/* TAGS: cpp big hashset */
/* VERIFY_OPTS: -o nofail:malloc */
/* CC_OPTS: -I$SRC_ROOT/bricks/ */
#include <brick-hashset>
#include <pthread.h>
#include <cassert>

// V: t2c2o0sc    CC_OPT: -DTHRS=2 -DCNT=2 -DOVERLAP=0
// V: t2c2o1sc    CC_OPT: -DTHRS=2 -DCNT=2 -DOVERLAP=1
// V: t2c4o2sc    CC_OPT: -DTHRS=2 -DCNT=4 -DOVERLAP=2
// V: t3c1o2sc    CC_OPT: -DTHRS=3 -DCNT=1 -DOVERLAP=2
// V: t2c2o0tso4  CC_OPT: -DTHRS=2 -DCNT=2 -DOVERLAP=0 V_OPT: --relaxed-memory tso:4    TAGS: tso
// V: t2c2o1tso4  CC_OPT: -DTHRS=2 -DCNT=2 -DOVERLAP=1 V_OPT: --relaxed-memory tso:4    TAGS: tso
// V: t2c4o2tso4  CC_OPT: -DTHRS=2 -DCNT=4 -DOVERLAP=2 V_OPT: --relaxed-memory tso:4    TAGS: tso
// V: t3c1o2tso4  CC_OPT: -DTHRS=3 -DCNT=1 -DOVERLAP=2 V_OPT: --relaxed-memory tso:4    TAGS: tso
// V: t2c2o0tso16  CC_OPT: -DTHRS=2 -DCNT=2 -DOVERLAP=0 V_OPT: --relaxed-memory tso:16  TAGS: tso
// V: t2c2o1tso16  CC_OPT: -DTHRS=2 -DCNT=2 -DOVERLAP=1 V_OPT: --relaxed-memory tso:16  TAGS: tso
// V: t2c4o2tso16  CC_OPT: -DTHRS=2 -DCNT=4 -DOVERLAP=2 V_OPT: --relaxed-memory tso:16  TAGS: tso
// V: t3c1o2tso16  CC_OPT: -DTHRS=3 -DCNT=1 -DOVERLAP=2 V_OPT: --relaxed-memory tso:16  TAGS: tso

using namespace brick::hashset;
using brick::t_hashset::test_hasher;

using HashSet = CompactConcurrent< int, test_hasher< int > >;

void worker( int wid, HashSet hs ) {
    const int end = CNT + OVERLAP;
    for ( int i = 0; i < end; ++i ) {
        hs.insert( 1 + wid * CNT + i );
    }
}

__attribute__(( __annotate__( "divine.debugfn" ) ))
void dump( std::set< int > &seen ) noexcept {
    printf( "{ " );
    for ( int x : seen ) {
        printf( "%d ", x );
    }
    printf( "}" );
}

int main() {
    HashSet hs;
    pthread_t thrs[THRS - 1];
    for ( int i = 0; i < THRS - 1; ++i ) {
        std::pair< int, HashSet * > p( i + 1, &hs );
        pthread_create( &thrs[i], nullptr, []( void *hs ) -> void * {
                auto p = *reinterpret_cast< std::pair< int, HashSet * > * >( hs );
                worker( p.first, *p.second );
                return nullptr;
            }, &p );
    }
    worker( 0, hs );
    for ( int i = 0; i < THRS - 1; ++i ) {
        pthread_join( thrs[i], nullptr );
    }

    std::set< int > seen;
    for ( auto &c : hs._s->table[ hs._s->currentRow ] ) {
        if ( c.empty() )
            continue;
        auto r = seen.insert( c.copy() );
        assert( r.second );
    }
    dump( seen );
    assert( seen.size() == CNT * THRS + OVERLAP );
    for ( int i = 1; i <= CNT * THRS; ++i ) {
        assert( seen.find( i ) != seen.end() );
    }
}
