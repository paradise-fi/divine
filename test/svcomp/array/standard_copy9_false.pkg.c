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

int main( ) {
  int a1[SIZE];
  int a2[SIZE];
  int a3[SIZE];
  int a4[SIZE];
  int a5[SIZE];
  int a6[SIZE];
  int a7[SIZE];
  int a8[SIZE];
  int a9[SIZE];
  int a0[SIZE];

  int a;
  for ( a = 0 ; a < SIZE ; a++ ) {
    a1[a] = __VERIFIER_nondet_int();
    a9[a] = __VERIFIER_nondet_int();
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
    a5[i] = a4[i];
  }
  for ( i = 0 ; i < SIZE ; i++ ) {
    a6[i] = a5[i];
  }
  for ( i = 0 ; i < SIZE ; i++ ) {
    a7[i] = a6[i];
  }
  for ( i = 0 ; i < SIZE ; i++ ) {
    a8[i] = a7[i];
  }
  for ( i = 0 ; i < SIZE ; i++ ) {
    a0[i] = a8[i];
  }
  for ( i = 0 ; i < SIZE ; i++ ) {
    a0[i] = a9[i];
  }

  int x;
  for ( x = 0 ; x < SIZE ; x++ ) {
    __VERIFIER_assert(  a1[x] == a0[x]  ); /* ERROR */
  }
  return 0;
}

