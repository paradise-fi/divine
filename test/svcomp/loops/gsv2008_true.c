/* TAGS: c sym */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc */
// Source: Sumit Gulwani, Saurabh Srivastava, Ramarathnam Venkatesan: "Program
// Analysis as Constraint Solving", PLDI 2008.
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
    int x,y;
    x = -50;
    y = __VERIFIER_nondet_int();
    if (!(-NUM < y && y < NUM)) return 0;
    while (x < 0) {
	x = x + y;
	y++;
    }
    __VERIFIER_assert(y > 0);
    return 0;
}
