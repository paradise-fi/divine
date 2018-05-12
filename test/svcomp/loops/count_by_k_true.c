/* TAGS: c sym todo */
/* VERIFY_OPTS: --symbolic */
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
    int i;
    int k;
    k = __VERIFIER_nondet_int();
    if (!(0 <= k && k <= 10)) return 0;
    for (i = 0; i < LARGE_INT*k; i += k) ;
    __VERIFIER_assert(i == LARGE_INT*k);
    return 0;
}
