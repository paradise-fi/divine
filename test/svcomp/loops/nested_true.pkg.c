/* TAGS: c sym */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc */
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

// V: small.10 CC_OPT: -DNUM=11
// V: big.100 CC_OPT: -DNUM=101 TAGS: ext
// V: big.1000 CC_OPT: -DNUM=1001 TAGS: big
// V: big.10000 CC_OPT: -DNUM=10001 TAGS: big
// V: big.100000 CC_OPT: -DNUM=100001 TAGS: big

int main() {
    int n = __VERIFIER_nondet_int();
    int m = __VERIFIER_nondet_int();
    int k = 0;
    int i,j;
    if (!(10 <= n && n <= NUM)) return 0;
    if (!(10 <= m && m <= NUM)) return 0;
    for (i = 0; i < n; i++) {
	for (j = 0; j < m; j++) {
	    k ++;
	}
    }
    __VERIFIER_assert(k >= 100);
    return 0;
}
