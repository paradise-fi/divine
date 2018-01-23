/* TAGS: c threads */
/* CC_OPTS: */

#include <pthread.h>
#include <assert.h>

// V: i2.error CC_OPT: -DBUG -DNUM=2 RESULT: error
// V: i2.valid CC_OPT: -DNUM=2       RESULT: valid
// V: i5.error CC_OPT: -DBUG -DNUM=5 RESULT: error TAGS: big
// V: i5.valid CC_OPT: -DNUM=5       RESULT: valid TAGS: big
// V: i7.error CC_OPT: -DBUG -DNUM=7 RESULT: error TAGS: big
// V: i7.valid CC_OPT: -DNUM=7       RESULT: valid TAGS: big

int i=1, j=1;
int fib( int n ) { return n <= 1 ? 1 : fib( n - 1 ) + fib( n - 2 ); }

void *t1( void* arg )
{
    for ( int k = 0; k < NUM; k++ )
        i += j;
    return 0;
}

void *t2( void* arg )
{
    for ( int k = 0; k < NUM; k++ )
        j += i;
    return 0;
}

int main()
{
    pthread_t id1, id2;
    int max = fib( 2 * NUM + 1 );

    pthread_create(&id1, NULL, t1, NULL);
    pthread_create(&id2, NULL, t2, NULL);
#ifdef BUG
    assert( i < max && j < max ); /* ERROR */
#else
    assert( i <= max && j <= max );
#endif

    return 0;
}
