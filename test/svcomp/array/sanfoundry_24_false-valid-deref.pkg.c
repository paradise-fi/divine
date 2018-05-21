/* TAGS: c sym todo */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc */
/* CC_OPTS: */

// V: small.10 CC_OPT: -DSIZE=10
// V: small.100 CC_OPT: -DSIZE=100
// V: small.1000 CC_OPT: -DSIZE=1000
// V: big.10000 CC_OPT: -DSIZE=10000 TAGS: big
// V: big.100000 CC_OPT: -DSIZE=100000 TAGS: big
extern void __VERIFIER_error() __attribute__ ((__noreturn__));
void __VERIFIER_assert(int cond) { if(!(cond)) { ERROR: __VERIFIER_error(); } }
extern int __VERIFIER_nondet_int(void);

/*
 * Adapted from http://www.sanfoundry.com/c-programming-examples-arrays/
 * C Program to Print the Number of Odd & Even Numbers in an Array
 */

void printEven( int i ) {
  __VERIFIER_assert(  ( i % 2 ) == 0  );
  // printf( "%d" , i );
}

void printOdd( int i ) {
  __VERIFIER_assert(  ( i % 2 ) != 0  );
  // printf( "%d" , i );
}

int main()
{
    int array[SIZE];
    int i;
    int num = __VERIFIER_nondet_int();

    //printf("Even numbers in the array are - ");
    for (i = 0; i < num; i++) // use of uninitialized num
    {
        if (array[i] % 2 == 0)
        {
            printEven( array[i] );
        }
    }
    //printf("\n Odd numbers in the array are -");
    for (i = 0; i < num; i++)
    {
        if (array[i] % 2 != 0)
        {
            printOdd( array[i] );
        }
    }
  return 0;
}
