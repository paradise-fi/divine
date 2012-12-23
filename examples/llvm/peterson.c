/*
 * This program implements the Peterson's mutual exclusion algorithm. When
 * compiled with -DBUG, the algorithm is incorrect but runs without tripping
 * assertions more often than not.
 *
 * Verify with:
 *  $ divine compile --llvm [--cflags=" < flags > "] peterson.c
 *  $ divine reachability peterson.bc --ignore-deadlocks [-d]
 * Execute with:
 *  $ clang [ < flags > ] -lpthread -o peterson.exe peterson.c
 *  $ ./peterson.exe
 */

#include <pthread.h>
#include <cstdlib>

// For native execution (in future we will provide cassert).
#ifndef DIVINE
#include <cassert>
#endif

struct state {
    int flag[2];
    int turn;
    volatile int in_critical[2];
};

struct p {
    int id;
    pthread_t ptid;
    struct state *s;
};

enum AP { wait1, critical1, wait2, critical2 };

// LTL(progress, G(wait1 -> F(critical1)) && G(wait2 -> F(critical2)));
/* TODO: progress fails due to lack of fairness */
// LTL(exclusion, G(!(critical1 && critical2))); // OK

void * thread( void *in ) __attribute__((noinline));
void * thread( void *in ) {
	struct p* p = (struct p*) in;
#ifdef BUG
    p->s->flag[p->id] = 0;
#else // correct
    p->s->flag[p->id] = 1;
#endif
    p->s->turn = 1 - p->id;

    // ap( p->id ? wait1 : wait2 );
    while ( p->s->flag[1 - p->id] == 1 && p->s->turn == 1 - p->id ) ;

    p->s->in_critical[p->id] = 1;
    // ap( p->id ? critical1 : critical2 );
    assert( !p->s->in_critical[1 - p->id] );
    p->s->in_critical[p->id] = 0;

    p->s->flag[p->id] = 0;
    
    return NULL;
}

int main() {
    struct state *s = (struct state *) malloc( sizeof( struct state ) );
    struct p *one = (struct p*) malloc( sizeof( struct p ) ),
             *two = (struct p*) malloc( sizeof( struct p ) );
    if (!s || !one || !two)
        return 1;

    one->s = two->s = s;
    one->id = 0;
    two->id = 1;

    s->flag[0]   = 0;
    s->flag[1]   = 0;
    s->in_critical[0] = 0;
    s->in_critical[1] = 0;
    pthread_create( &one->ptid, NULL, thread, one );
    pthread_create( &two->ptid, NULL, thread, two );
    pthread_join( one->ptid, NULL );
    pthread_join( two->ptid, NULL );
    return 0;
}
