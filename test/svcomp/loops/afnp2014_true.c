/* TAGS: c sym todo */
/* VERIFY_OPTS: --symbolic */
// Source: E. De Angelis, F. Fioravanti, J. A. Navas, M. Proietti:
// "Verification of Programs by Combining Iterated Specialization with
// Interpolation", HCVS 2014
extern void __VERIFIER_error(void);
extern void __VERIFIER_assume(int);
void __VERIFIER_assert(int cond) {
  if (!(cond)) {
      ERROR: __VERIFIER_error();
  }
  return;
}
int __VERIFIER_nondet_int();
#define LARGE_INT 1000000

int main() {
    int x = 1;
    int y = 0;
    while (y < 1000 && __VERIFIER_nondet_int()) {
        x = x + y;
        y = y + 1;
    }
    __VERIFIER_assert(x >= y);
    return 0;
}
