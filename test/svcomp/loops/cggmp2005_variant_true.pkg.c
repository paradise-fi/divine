/* TAGS: c sym */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc */
// Source: A. Costan, S. Gaubert, E. Goubault, M. Martel, S. Putot: "A Policy
// Iteration Algorithm for Computing Fixed Points in Static Analysis of
// Programs", CAV 2005
extern void __VERIFIER_error(void);
extern void __VERIFIER_assume(int);
void __VERIFIER_assert(int cond) {
  if (!(cond)) {
      ERROR: __VERIFIER_error();
  }
  return;
}
extern int __VERIFIER_nondet_int(void);

// V: small.10 CC_OPT: -DNUM=10
// V: small.100 CC_OPT: -DNUM=100
// V: big.1000 CC_OPT: -DNUM=1000 TAGS: ext
// V: big.10000 CC_OPT: -DNUM=10000 TAGS: big
// V: big.100000 CC_OPT: -DNUM=100000 TAGS: big

int main() {
    int lo, mid, hi;
    lo = 0;
    mid = __VERIFIER_nondet_int();
    if (!(mid > 0 && mid <= NUM)) return 0;
    hi = 2*mid;

    while (mid > 0) {
        lo = lo + 1;
        hi = hi - 1;
        mid = mid - 1;
    }
    __VERIFIER_assert(lo == hi);
    return 0;
}
