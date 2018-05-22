/* TAGS: c sym todo */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc */
/* CC_OPTS: */

// V: v.10 CC_OPT: -DSIZE=10
// V: v.100 CC_OPT: -DSIZE=100
// V: v.1000 CC_OPT: -DSIZE=1000
// V: v.10000 CC_OPT: -DSIZE=10000 TAGS: big
// V: v.100000 CC_OPT: -DSIZE=100000 TAGS: big

/*
 * Benchmarks contributed by Shrawan Kumar, TCS Innovation labs, Pune
 *
 * It implements partial copy and
 * check property accordingly
 */

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
    __VERIFIER_assert(  a1[x] == a2[x]  ); /* ERROR */
  }
  return 0;
}

