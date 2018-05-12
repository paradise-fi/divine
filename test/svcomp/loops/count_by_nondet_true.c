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
    int i = 0;
    int k = 0;
    while(i < LARGE_INT) {
        int j = __VERIFIER_nondet_int();
        if (!(1 <= j && j < LARGE_INT)) return 0;
        i = i + j;
        k ++;
    }
    __VERIFIER_assert(k <= LARGE_INT);
    return 0;
}
