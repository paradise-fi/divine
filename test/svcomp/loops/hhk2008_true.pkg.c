/* TAGS: c sym */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc */
/* CC_OPTS: */

// Source: Thomas A. Henzinger, Thibaud Hottelier, Laura Kovacs: "Valigator:
// A verification Tool with Bound and Invariant Generation", LPAR 2008
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
// V: big.1000 CC_OPT: -DNUM=1000 TAGS: big
// V: big.10000 CC_OPT: -DNUM=10000 TAGS: big
// V: big.100000 CC_OPT: -DNUM=100000 TAGS: big

int main() {
    int a = __VERIFIER_nondet_int();
    int b = __VERIFIER_nondet_int();
    int res, cnt;
    if (!(a <= NUM)) return 0;
    if (!(0 <= b && b <= NUM)) return 0;
    res = a;
    cnt = b;
    while (cnt > 0) {
	cnt = cnt - 1;
	res = res + 1;
    }
    __VERIFIER_assert(res == a + b);
    return 0;
}
