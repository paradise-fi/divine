/* TAGS: c threads */
/* CC_OPTS: */
/* VERIFY_OPTS: -o nofail:malloc */

#include <stdlib.h>
#include <pthread.h>
#include <assert.h>

// V: 016-4-2 CC_OPT: -DSIZE=16  -DMAX=4 -DNUM_THREADS=2
// V: 016-4-3 CC_OPT: -DSIZE=16  -DMAX=4 -DNUM_THREADS=3 TAGS: big
// V: 128-4-2 CC_OPT: -DSIZE=128 -DMAX=4 -DNUM_THREADS=2
// V: 128-4-3 CC_OPT: -DSIZE=128 -DMAX=4 -DNUM_THREADS=3 TAGS: big
// V: 128-8-2 CC_OPT: -DSIZE=128 -DMAX=8 -DNUM_THREADS=2 TAGS: ext
// note: svcomp uses 128-4-13

int table[SIZE];
pthread_mutex_t  cas_mutex[SIZE];

pthread_t  tids[NUM_THREADS];


int cas(int * tab, int h, int val, int new_val)
{
  int ret_val = 0;
  pthread_mutex_lock(&cas_mutex[h]);
  
 
  if ( tab[h] == val ) {
    tab[h] = new_val;
    ret_val = 1;
  }

  pthread_mutex_unlock(&cas_mutex[h]);

  
  return ret_val;
} 



void * thread_routine(void * arg)
{
  int tid;
  int m = 0, w, h;
  tid = *((int *)arg);
  
  while(1){
    if ( m < MAX ){
      w = (++m) * 11 + tid;
    }
    else{
      pthread_exit(NULL);
    }
    
    h = (w * 7) % SIZE;
    assert( h >= 0 );

    while ( cas(table, h, 0, w) == 0){
      h = (h+1) % SIZE;
    }
  }

}


int main()
{
  int i, arg;

  for (i = 0; i < SIZE; i++)
    pthread_mutex_init(&cas_mutex[i], NULL);

  for (i = 0; i < NUM_THREADS; i++){
    arg=i;
    pthread_create(&tids[i], NULL,  thread_routine, &arg);
  }

  for (i = 0; i < NUM_THREADS; i++){
    pthread_join(tids[i], NULL);
  }

  for (i = 0; i < SIZE; i++){
    pthread_mutex_destroy(&cas_mutex[i]);
  }

}
