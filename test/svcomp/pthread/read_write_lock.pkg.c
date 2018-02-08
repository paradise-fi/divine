/* TAGS: c threads */
/* CC_OPTS: */
/* VERIFY_OPTS: -o nofail:malloc */

/* Testcase from Threader's distribution. For details see:
   http://www.model.in.tum.de/~popeea/research/threader

   This file is adapted from the example introduced in the paper:
   Thread-Modular Verification for Shared-Memory Programs 
   by Cormac Flanagan, Stephen Freund, Shaz Qadeer.
*/

#include <stdlib.h>
#include <pthread.h>
#include <assert.h>

void __VERIFIER_assume(int);
void __VERIFIER_atomic_begin(void);
void __VERIFIER_atomic_end(void);

// V: valid
// V: error CC_OPT: -DBUG

int w=0, r=0, x, y;

void *writer(void *arg) { //writer
  __VERIFIER_atomic_begin();
  __VERIFIER_assume (w==0 && r==0);
  w = 1;
  __VERIFIER_atomic_end();
  x = 3;
  w = 0;
  return 0;
}

void *reader(void *arg) { //reader
  int l;
  __VERIFIER_atomic_begin();
  __VERIFIER_assume (w==0);
  r = r + 1;
  __VERIFIER_atomic_end();
  l = x;
  y = l;
  assert(y == x); /* ERR_error */
#ifdef BUG
  l = r-1;
  r = l;
#else
  __sync_fetch_and_add( &r, -1 );
#endif
  return 0;
}

int main() {
  pthread_t t1, t2, t3, t4;
  pthread_create(&t1, 0, writer, 0);
  pthread_create(&t2, 0, reader, 0);
  pthread_create(&t3, 0, writer, 0);
  pthread_create(&t4, 0, reader, 0);
  pthread_join(t1, 0);
  pthread_join(t2, 0);
  pthread_join(t3, 0);
  pthread_join(t4, 0);
  return 0;
}
