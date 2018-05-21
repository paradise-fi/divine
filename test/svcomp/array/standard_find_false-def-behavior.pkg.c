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
extern int __VERIFIER_nondet_int();

int main ( ) {
  int a[SIZE]; int e = __VERIFIER_nondet_int();
  int i = 0;
  while( i < SIZE && a[i] != e ) {
    i = i + 1;
  }

  int x;
  for ( x = 0 ; x < i ; x++ ) {
    __VERIFIER_assert(  a[x] != e  ); /* ERROR */
  }
  return 0;
}
