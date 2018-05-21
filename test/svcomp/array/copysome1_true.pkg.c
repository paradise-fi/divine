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
extern int __VERIFIER_nondet_int(void);

int main( ) {
  int a1[SIZE];
  int a2[SIZE];

  int i;
  int z;
  z = SIZE/2 + 1;

  for ( i = 0 ; i < SIZE ; i++ ) {
         a1[i] = __VERIFIER_nondet_int();
	 a2[i] = __VERIFIER_nondet_int();
  }

  for ( i = 0 ; i < SIZE ; i++ ) {
      if (i != z)
         a2[i] = a1[i];
  }

  int x;
  for ( x = 0 ; x < SIZE ; x++ ) {
      if (x != z)
    __VERIFIER_assert(  a1[x] == a2[x]  );
  }
  return 0;
}

