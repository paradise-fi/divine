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

int main ( ) {
  int a [SIZE];
  int b [SIZE];
  int i = 0;
  while ( i < SIZE ) {
    a[i] = 42;
    i = i + 1;
  }

  for ( i = 0 ; i < SIZE ; i++ ) {
    b[i] = a[i];
  }

  for ( i = 0 ; i < SIZE ; i++ ) {
    a[i] = b[i] + i;
  }

  int x;
  for ( x = 0 ; x < SIZE ; x++ ) {
    __VERIFIER_assert(  b[x] == 42 + x  ); /* ERROR */
  }
  return 0;
}
