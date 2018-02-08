/* TAGS: c threads */
/* CC_OPTS: */
/* VERIFY_OPTS: -o nofail:malloc */

#include <stdlib.h>
#include <pthread.h>
#include <assert.h>

/* 7 takes over 50G of RAM, svcomp uses i=16 */
// V: i2 CC_OPT: -DSIGMA=2
// V: i5 CC_OPT: -DSIGMA=5 TAGS: big

int *array;
int array_index=-1;

void *thread(void * arg)
{
	array[array_index] = 1;
	return 0;
}


int main()
{
	int tid, sum;
	pthread_t *t;

	t = (pthread_t *)calloc(SIGMA, sizeof(pthread_t));
	array = (int *)calloc(SIGMA, sizeof(int));

        if ( !t || !array )
            return 0;

	for (tid=0; tid<SIGMA; tid++) {
	        array_index++;
		pthread_create(&t[tid], 0, thread, 0);
	}

	for (tid=0; tid<SIGMA; tid++) {
		pthread_join(t[tid], 0);
	}

	for (tid=sum=0; tid<SIGMA; tid++) {
		sum += array[tid];
	}

	assert(sum == SIGMA); /* ERROR */

	return 0;
}
