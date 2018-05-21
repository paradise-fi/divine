/* TAGS: c sym */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc */
/* CC_OPTS: */

extern void __VERIFIER_error() __attribute__ ((__noreturn__));
extern void __VERIFIER_assert(int);
extern int __VERIFIER_nondet_int(void);

// V: small.100 CC_OPT: -DNUM=100
// V: big.1000 CC_OPT: -DNUM=1000 TAGS: ext

int main(void) {
  int A[NUM];
  int i;

  for (i = 0; i < NUM-1; i++) {
    A[i] = __VERIFIER_nondet_int();
  }

  A[NUM-1] = 0;

  for (i = 0; A[i] != 0; i++) {
  }

  __VERIFIER_assert(i <= NUM);
}
