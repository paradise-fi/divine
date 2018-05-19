/* TAGS: c sym */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc */
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
    int n = 0;
    int k = __VERIFIER_nondet_int();
    if (!(k <= NUM && k >= -NUM)) return 0;
    for(i = 0; i < 2*k; i++) {
        if (i % 2 == 0) {
            n ++;
        }
    }
    __VERIFIER_assert(k < 0 || n == k);
    return 0;
}
