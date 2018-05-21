/* TAGS: c todo */
/* VERIFY_OPTS: --sequential -o nofail:malloc */
/* CC_OPTS: */

// V: small.10 CC_OPT: -DSIZE=10
// V: small.100 CC_OPT: -DSIZE=100
// V: small.1000 CC_OPT: -DSIZE=1000
// V: big.10000 CC_OPT: -DSIZE=10000 TAGS: big
// V: big.100000 CC_OPT: -DSIZE=100000 TAGS: big
extern void __VERIFIER_error() __attribute__ ((__noreturn__));
void __VERIFIER_assert(int cond) { if(!(cond)) { ERROR: __VERIFIER_error(); /* ERROR */ } }

int main( ) {
  int a[N];
  int i;
  int r = 1;
  for ( i = 1 ; i < N && r ; i++ ) {
    int j;
    for ( j = i-1 ; j >= 0 && r ; j-- ) {
      if ( a[i] == a[j] ) {
        r = 1;
      }
    }
  }

  if ( r ) {
    int x;
    int y;
    for ( x = 0 ; x < N ; x++ ) {
      for ( y = x+1 ; y < N ; y++ ) {
        __VERIFIER_assert(  a[x] != a[y]  ); /* ERROR */
      }
    }
  }
  return 0;
}
