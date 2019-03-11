/* TAGS: c sym */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc */
/* CC_OPTS: */

// V: v.10 CC_OPT: -DSIZE=10
// V: v.100 CC_OPT: -DSIZE=100
// V: v.1000 CC_OPT: -DSIZE=1000
// V: v.10000 CC_OPT: -DSIZE=10000 TAGS: big
// V: v.100000 CC_OPT: -DSIZE=100000 TAGS: big
extern void __VERIFIER_error() __attribute__ ((__noreturn__));
void __VERIFIER_assert(int cond) { if(!(cond)) { ERROR: __VERIFIER_error(); } }
extern int __VERIFIER_nondet_int();

int main( ) {
  int a1[SIZE];
  int a2[SIZE];
  int a3[SIZE];
  int a4[SIZE];
  int a5[SIZE];
  int a6[SIZE];

  int a;
  for ( a = 0 ; a < SIZE ; a++ ) {
    a1[a] = __VERIFIER_nondet_int();
    a5[a] = __VERIFIER_nondet_int();
  }

  int i;
  for ( i = 0 ; i < SIZE ; i++ ) {
    a2[i] = a1[i];
  }
  for ( i = 0 ; i < SIZE ; i++ ) {
    a3[i] = a2[i];
  }
  for ( i = 0 ; i < SIZE ; i++ ) {
    a4[i] = a3[i];
  }
  for ( i = 0 ; i < SIZE ; i++ ) {
    a6[i] = a4[i];
  }
  for ( i = 0 ; i < SIZE ; i++ ) {
    a6[i] = a5[i];
  }

  int x;
  for ( x = 0 ; x < SIZE ; x++ ) {
    __VERIFIER_assert(  a1[x] == a6[x]  ); /* ERROR */
  }
  return 0;
}
