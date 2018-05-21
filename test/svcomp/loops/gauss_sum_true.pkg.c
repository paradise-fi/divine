/* TAGS: c sym */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

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
// V: small.100 CC_OPT: -DNUM=100 TAGS: ext
// V: big.1000 CC_OPT: -DNUM=1000 TAGS: big
// V: big.10000 CC_OPT: -DNUM=10000 TAGS: big
// V: big.100000 CC_OPT: -DNUM=100000 TAGS: big

int main() {
    int n, sum, i;
    n = __VERIFIER_nondet_int();
    if ( n < 1 || n > NUM ) return 0;
    sum = 0;
    for(i = 1; i <= n; i++) {
        sum = sum + i;
    }
    __VERIFIER_assert(2*sum == n*(n+1));
    return 0;
}
