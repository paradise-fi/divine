/* TAGS: c sym */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc */
/* CC_OPTS: */

// V: small.10 CC_OPT: -DSIZE=10
// V: small.100 CC_OPT: -DSIZE=100
// V: small.1000 CC_OPT: -DSIZE=1000
// V: big.10000 CC_OPT: -DSIZE=10000
// V: big.100000 CC_OPT: -DSIZE=100000
extern void __VERIFIER_error() __attribute__ ((__noreturn__));
extern int __VERIFIER_nondet_int(void);
void __VERIFIER_assert(int cond) { if(!(cond)) { ERROR: __VERIFIER_error(); } }

#define SIZE 100000

int main ( ) {
  int a[SIZE];
  int marker = __VERIFIER_nondet_int();
  int pos = __VERIFIER_nondet_int();
  if ( pos >= 0 && pos < SIZE ) {
    a[ pos ] = marker;

    int i = 0;
    while( a[ i ] != marker ) {
      i = i + 1;
    }

    __VERIFIER_assert(  i <= pos  );
  }
  return 0;
}
