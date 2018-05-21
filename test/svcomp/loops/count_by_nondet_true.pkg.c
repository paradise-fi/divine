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
// V: small.100 CC_OPT: -DNUM=100
// V: big.1000 CC_OPT: -DNUM=1000 TAGS: ext
// V: big.10000 CC_OPT: -DNUM=10000 TAGS: big
// V: big.100000 CC_OPT: -DNUM=100000 TAGS: big

int main() {
    int i = 0;
    int k = 0;
    while(i < NUM) {
        int j = __VERIFIER_nondet_int();
        if (!(1 <= j && j < NUM)) return 0;
        i = i + j;
        k ++;
    }
    __VERIFIER_assert(k <= NUM);
    return 0;
}
