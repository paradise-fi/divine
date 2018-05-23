/* TAGS: c sym */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc -o ignore:control */
/* CC_OPTS: */

// V: v.10 CC_OPT: -DSIZE=10
// V: v.100 CC_OPT: -DSIZE=100 TAGS: big
// V: v.1000 CC_OPT: -DSIZE=1000 TAGS: big
// V: v.10000 CC_OPT: -DSIZE=10000 TAGS: big
// V: v.100000 CC_OPT: -DSIZE=100000 TAGS: big
extern void __VERIFIER_error() __attribute__ ((__noreturn__));
extern int __VERIFIER_nondet_int(void);
void __VERIFIER_assert(int cond) { if(!(cond)) { ERROR: __VERIFIER_error(); } }

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
    for (i = 0; i < num; i++)
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
