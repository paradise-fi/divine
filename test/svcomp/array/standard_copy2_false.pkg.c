/* TAGS: c sym */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc */
/* CC_OPTS: */

// V: v.10 CC_OPT: -DSIZE=10
// V: v.100 CC_OPT: -DSIZE=100
// V: v.1000 CC_OPT: -DSIZE=1000 TAGS: todo
// V: v.10000 CC_OPT: -DSIZE=10000 TAGS: todo big
// V: v.100000 CC_OPT: -DSIZE=100000 TAGS: todo big
extern void __VERIFIER_error() __attribute__ ((__noreturn__));
void __VERIFIER_assert(int cond) { if(!(cond)) { ERROR: __VERIFIER_error(); } }
extern int __VERIFIER_nondet_int();

int main( ) {
  int a1[SIZE];
  int a2[SIZE];
  int a3[SIZE];

  int a;
  for ( a = 0 ; a < SIZE ; a++ ) {
    a1[a] = __VERIFIER_nondet_int();
    a2[a] = __VERIFIER_nondet_int();
  }

  int i;
  for ( i = 0 ; i < SIZE ; i++ ) {
    a3[i] = a1[i];
  }
  for ( i = 0 ; i < SIZE ; i++ ) {
    a3[i] = a2[i];
  }

  int x;
  for ( x = 0 ; x < SIZE ; x++ ) {
    __VERIFIER_assert(  a1[x] == a3[x]  ); /* ERROR */
  }
  return 0;
}

