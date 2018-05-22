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

int main ( ) {
  int a[SIZE];
  int i = 0;
  while ( i < SIZE ) {
    a[i] = 42;
    i = i + 1;
  }

  int x;
  for ( x = 0 ; x < SIZE ; x++ ) {
    __VERIFIER_assert(  a[x] == 43  ); /* ERROR */
  }
  return 0;
}
