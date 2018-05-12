/* TAGS: c */
/* VERIFY_OPTS: --sequential -o nofail:malloc */
// Source: A. Costan, S. Gaubert, E. Goubault, M. Martel, S. Putot: "A Policy
// Iteration Algorithm for Computing Fixed Points in Static Analysis of
// Programs", CAV 2005
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
    int i,j;
    i = 1;
    j = 10;
    while (j >= i) {
        i = i + 2;
        j = -1 + j;
    }
    __VERIFIER_assert(j == 6);
    return 0;
}
