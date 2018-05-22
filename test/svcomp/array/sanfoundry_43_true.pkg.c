/* TAGS: c todo */
/* VERIFY_OPTS: --sequential -o nofail:malloc */
/* CC_OPTS: */

// V: v.10 CC_OPT: -DSIZE=10
// V: v.100 CC_OPT: -DSIZE=100
// V: v.1000 CC_OPT: -DSIZE=1000
// V: v.10000 CC_OPT: -DSIZE=10000 TAGS: big
// V: v.100000 CC_OPT: -DSIZE=100000 TAGS: big
extern void __VERIFIER_error() __attribute__ ((__noreturn__));
void __VERIFIER_assert(int cond) { if(!(cond)) { ERROR: __VERIFIER_error(); } }

/*
 * Adapted from http://www.sanfoundry.com/c-programming-examples-arrays/
 * C Program to Increment every Element of the Array by one & Print Incremented Array
 */

void incrementArray(int src[SIZE] , int dst[SIZE])
{
    int i;
    for (i = 0; i < SIZE; i++) {
        dst[i] = src[i]+1;     // this alters values in array in main()
    }
}

int main()
{
    int src[SIZE];
    int dst[SIZE];

    incrementArray( src , dst );

    int x;
    for ( x = 0 ; x < SIZE ; x++ ) {
      src[ x ] = dst[ x ]-1;
    }
  return 0;
}


