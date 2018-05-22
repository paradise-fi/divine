/* TAGS: c sym */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc */
/* CC_OPTS: */

// V: v.10 CC_OPT: -DSIZE=10
// V: v.100 CC_OPT: -DSIZE=100 TAGS: ext
// V: v.1000 CC_OPT: -DSIZE=1000 TAGS: big
// V: v.10000 CC_OPT: -DSIZE=10000 TAGS: big
// V: v.100000 CC_OPT: -DSIZE=100000 TAGS: big

extern void __VERIFIER_error() __attribute__ ((__noreturn__));
extern int __VERIFIER_nondet_int(void);
void __VERIFIER_assert(int cond) { if(!(cond)) { ERROR: __VERIFIER_error(); } }

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
