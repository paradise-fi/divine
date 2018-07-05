#ifndef REGTEST
#include <threads.h>
#include <pthread.h>
#include <assert.h>

void tss_delete(tss_t key)
{
    int r = pthread_key_delete(key);
    assert(r == 0);
}
#endif

#ifdef TEST
#include "_PDCLIB_test.h"

/* Tested in tss_get.c */
int main( void )
{
    return TEST_RESULTS;
}

#endif
