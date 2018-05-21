/* TAGS: c todo */
/* VERIFY_OPTS: --sequential -o nofail:malloc */
/* CC_OPTS: */

// V: small.10 CC_OPT: -DSIZE=10
// V: small.100 CC_OPT: -DSIZE=100
// V: small.1000 CC_OPT: -DSIZE=1000
// V: big.10000 CC_OPT: -DSIZE=10000 TAGS: big
// V: big.100000 CC_OPT: -DSIZE=100000 TAGS: big
extern void __VERIFIER_error() __attribute__ ((__noreturn__));
void __VERIFIER_assert(int cond) { if(!(cond)) { ERROR: __VERIFIER_error(); } }

/*
 * Adapted from http://www.sanfoundry.com/c-programming-examples-arrays/
 * C Program to Find the Largest Number in an Array
 */

int main()
{
    int array[SIZE];
    int i;
    int largest = array[0];
    for (i = 1; i < SIZE; i++)
    {
        if (largest < array[i])
            largest = array[i];
    }

    int x;
    for ( x = 0 ; x < SIZE ; x++ ) {
      __VERIFIER_assert(  largest >= array[ x ]  );
    }

    return 0;
}
