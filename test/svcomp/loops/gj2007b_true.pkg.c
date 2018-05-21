/* TAGS: c sym */
/* VERIFY_OPTS: --symbolic -sequential -o nofail:malloc */
/* CC_OPTS: */

// Source: Sumit Gulwani, Nebosja Jojic: "Program Verification as
// Probabilistic Inference", POPL 2007.
extern void __VERIFIER_error(void);
extern void __VERIFIER_assume(int);
void __VERIFIER_assert(int cond) {
  if (!(cond)) {
      ERROR: __VERIFIER_error();
  }
  return;
}
extern int __VERIFIER_nondet_int(void);

// V: small.5 CC_OPT: -DNUM=5
// V: small.10 CC_OPT: -DNUM=10
// V: small.100 CC_OPT: -DNUM=100 TAGS: ext
// V: big.1000 CC_OPT: -DNUM=1000 TAGS: big
// V: big.10000 CC_OPT: -DNUM=10000 TAGS: big
// V: big.100000 CC_OPT: -DNUM=100000 TAGS: big

int main() {
    int x = 0;
    int m = 0;
    int n = __VERIFIER_nondet_int();
    if ( n < -NUM || n > NUM ) return 0;

    while(x < n) {
	if(__VERIFIER_nondet_int()) {
	    m = x;
	}
	x = x + 1;
    }
    __VERIFIER_assert((m >= 0 || n <= 0));
    __VERIFIER_assert((m < n || n <= 0));
    return 0;
}
