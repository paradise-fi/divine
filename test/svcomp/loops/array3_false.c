/* TAGS: c todo sym */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc */

extern void __VERIFIER_error() __attribute__ ((__noreturn__));
extern int __VERIFIER_nondet_int(void);
extern void __VERIFIER_assert(int cond);

#define N 1024

int main(void) {
  int A[N];
  int i;

  for (i = 0; i < N; i++) {
    A[i] = __VERIFIER_nondet_int();
  }

  for (i = 0; A[i] != 0; i++) { /* ERROR */
    if (i >= N) {
      break;
    }
  }

  __VERIFIER_assert(i <= N);
}
