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
  int n0, n1;
  int i0 = 0;
  int k = 0;

  n0 = __VERIFIER_nondet_int();
  n1 = __VERIFIER_nondet_int();
  if (!(-LARGE_INT <= n0 && n0 < LARGE_INT)) return 0;
  if (!(-LARGE_INT <= n1 && n1 < LARGE_INT)) return 0;

  while( i0 < n0 ) {
    i0++;
    k++;
  }

  int i1 = 0;
  while( i1 < n1 ) {
    i1++;
    k++;
  }

  int j1 = 0;
  while( j1 < n0 + n1 ) {
      __VERIFIER_assert(k > 0);
      j1++;
      k--;
  }
}
