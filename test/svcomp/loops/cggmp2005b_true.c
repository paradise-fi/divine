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
    int i,j,k;
    i = 0;
    k = 9;
    j = -100;
    while (i <= 100) {
	i = i + 1;
	while (j < 20) {
	    j = i + j;
	}
	k = 4;
	while (k <= 3) {
	    k = k + 1;
	}
    }
    __VERIFIER_assert(k == 4);
    return 0;
}
