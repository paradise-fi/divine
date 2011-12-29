/*
 * This program implements the Peterson's mutual exclusion algorithm. When
 * compiled with -DBUG, the algorithm is incorrect but runs without tripping
 * assertions more often than not.
 *
 * verify with:
 *  $ clang -DDIVINE -c -emit-llvm [flags] -o peterson.bc peterson.c
 *  $ divine reachability peterson.bc --ignore-deadlocks
 * execute with:
 *  $ clang [flags] -o peterson.exe peterson.c
 *  $ ./peterson.exe
 */

#include "divine-llvm.h"

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

LTL(progress, G(wait1 -> F(critical1)) && G(wait2 -> F(critical2)));
/* TODO: progress fails due to lack of fairness */
LTL(exclusion, G(!(critical1 && critical2))); // OK

void thread( struct p *p ) __attribute__((noinline));
void thread( struct p *p ) {
#ifdef BUG
    p->s->flag[p->id] = 0;
#else // correct
    p->s->flag[p->id] = 1;
#endif
    p->s->turn = 1 - p->id;

    ap( p->id ? wait1 : wait2 );
    while ( p->s->flag[1 - p->id] == 1 && p->s->turn == 1 - p->id ) ;

    p->s->in_critical[p->id] = 1;
    ap( p->id ? critical1 : critical2 );
    assert( !p->s->in_critical[1 - p->id] );
    p->s->in_critical[p->id] = 0;

    p->s->flag[p->id] = 0;

}

int main() {
    struct state *s = malloc( sizeof( struct state ) );
    struct p *one = malloc( sizeof( struct p ) ),
             *two = malloc( sizeof( struct p ) );
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
