/* TAGS: c sym todo */
/* VERIFY_OPTS: --symbolic */
// Source: Dirk Beyer, Thomas A. Henzinger, Rupak Majumdar, Andrey
// Rybalchenko: "Path Invariants", PLDI 2007.
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
// V: small.10 CC_OPT: -DNUM=10 TAGS: ext
// V: big.100 CC_OPT: -DNUM=100 TAGS: big
// V: big.1000 CC_OPT: -DNUM=1000 TAGS: big
// V: big.100000 CC_OPT: -DNUM=100000 TAGS: big

int main() {
    int i, n, a, b;
    i = 0; a = 0; b = 0; n = __VERIFIER_nondet_int();
    if (!(n >= 0 && n <= NUM)) return 0;
    while (i < n) {
        if (__VERIFIER_nondet_int()) {
            a = a + 1;
            b = b + 2;
        } else {
            a = a + 2;
            b = b + 1;
        }
        i = i + 1;
    }
    __VERIFIER_assert(a + b == 3*n);
    return 0;
}
