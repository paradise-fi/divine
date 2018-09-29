/* TAGS: sym c */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.O0 CC_OPT: -O0 TAGS: min
// V: v.O1 CC_OPT: -O1
// V: v.O2 CC_OPT: -O2
// V: v.Os CC_OPT: -Os

#include <pthread.h>

unsigned int __VERIFIER_nondet_uint( void );
unsigned int val;

pthread_mutex_t m;

void set() { val = __VERIFIER_nondet_uint(); }

unsigned int get() { return val; }

void *t1( void * arg ) {
    pthread_mutex_lock(&m);
    set();
    pthread_mutex_unlock(&m);
}

void *t2( void * arg ) {
    pthread_mutex_lock(&m);
    get();
    pthread_mutex_unlock(&m);
}


int main(void) {
  pthread_t id1, id2;

  pthread_mutex_init(&m, 0);

  pthread_create(&id1, NULL, t1, NULL);
  pthread_create(&id2, NULL, t2, NULL);

  pthread_join(id1, NULL);
  pthread_join(id2, NULL);

  return 0;
}

