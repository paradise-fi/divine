/* TAGS: c big */
/* VERIFY_OPTS: --sequential -o nofail:malloc */
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
    for (i = 0; i < LARGE_INT; i++) ;
    __VERIFIER_assert(i == LARGE_INT);
    return 0;
}
