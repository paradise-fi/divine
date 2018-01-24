/* TAGS: c todo sym */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc */
extern void __VERIFIER_error() __attribute__ ((__noreturn__));
extern void __VERIFIER_assert(int);
extern int __VERIFIER_nondet_int(void);


#define N 1024

int main(void) {
  int A[N];
  int i;

  for (i = 0; i < N; i++) {
    A[i] = __VERIFIER_nondet_int();
  }

  for (i = 0; A[i] != 0; i++) {
    if (i >= N-1) {
      break;
    }
  }

  __VERIFIER_assert(i <= N);
}
