/* TAGS: c sym todo */
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
  int a1[N];
  int a2[N];
  int a3[N];
  int a4[N];
  int a5[N];
  int a6[N];
  int a7[N];
  int a8[N];
  int a9[N];
  int a0[N];

  int a;
  for ( a = 0 ; a < N ; a++ ) {
    a1[a] = __VERIFIER_nondet_int();
  }

  int i;
  for ( i = 0 ; i < N ; i++ ) {
    a2[i] = a1[i];
  }
  for ( i = 0 ; i < N ; i++ ) {
    a3[i] = a2[i];
  }
  for ( i = 0 ; i < N ; i++ ) {
    a4[i] = a3[i];
  }
  for ( i = 0 ; i < N ; i++ ) {
    a5[i] = a4[i];
  }
  for ( i = 0 ; i < N ; i++ ) {
    a6[i] = a5[i];
  }
  for ( i = 0 ; i < N ; i++ ) {
    a7[i] = a6[i];
  }
  for ( i = 0 ; i < N ; i++ ) {
    a8[i] = a7[i];
  }
  for ( i = 0 ; i < N ; i++ ) {
    a9[i] = a8[i];
  }
  for ( i = 0 ; i < N ; i++ ) {
    a0[i] = a9[i];
  }

  int x;
  for ( x = 0 ; x < N ; x++ ) {
    __VERIFIER_assert(  a1[x] == a0[x]  );
  }
  return 0;
}

