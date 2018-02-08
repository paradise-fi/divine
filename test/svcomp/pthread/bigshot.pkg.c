/* TAGS: c threads */
/* CC_OPTS: */
/* VERIFY_OPTS: -o nofail:malloc */

#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <assert.h>

// V: valid
// V: error CC_OPT: -DBUG

char *v;

void *thread1(void * arg)
{
  v = calloc(sizeof(char) * 8, 1);
  return 0;
}

void *thread2(void *arg)
{
  if (v) strcpy(v, "Bigshot");
  return 0;
}


int main()
{
  pthread_t t1, t2;

  pthread_create(&t1, 0, thread1, 0);
#ifndef BUG
  pthread_join(t1, 0);
#endif
  pthread_create(&t2, 0, thread2, 0);

#ifdef BUG
  pthread_join(t1, 0);
#endif
  pthread_join(t2, 0);

  assert(v[0] == 'B'); /* ERR_error */

  return 0;
}
