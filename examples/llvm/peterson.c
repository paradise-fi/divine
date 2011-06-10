/*
 * This program implements the Peterson's mutual exclusion algorithm. Different
 * combinations of build flags lead to different results (incorrect meaning the
 * assertion is violated):
 *
 *  -O0        correct,  5583 states
 *  -O0 -DBUG  correct,  5583 states
 *  -O2        correct,   170 states
 *  -O2 -DBUG: incorrect, 540 states
 *
 * compile with: clang -c -emit-llvm [flags] -o peterson.ll peterson.c
 */

struct state {
#ifndef BUG
    volatile
#endif
        int flag[2];
    int turn;
    volatile int in_critical[2];
};

void threads( struct state *s ) {
    int id = thread_create();
    s->flag[id] = 1;
    s->turn = 1 - id;
    while ( s->flag[id] == 1 && s->turn == 1 - id ) ;
    s->in_critical[id] = 1;
    assert( !s->in_critical[1 - id] );
    s->flag[id] = 0;
}

int main() {
    struct state s;
    struct state * volatile _s = &s;

    s.flag[0]   = 0;
    s.flag[1]   = 0;
    s.in_critical[0] = 0;
    s.in_critical[1] = 0;
    threads( _s );

    return 0;
}
