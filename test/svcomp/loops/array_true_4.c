/* TAGS: c todo sym */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc */
extern void __VERIFIER_error() __attribute__ ((__noreturn__));
extern void __VERIFIER_assert(int);
extern int __VERIFIER_nondet_int(void);


#define N 1024

int main(void) {
  int A[N];
  int i;

  for (i = 0; i < N-1; i++) {
    A[i] = __VERIFIER_nondet_int();
  }

  A[N-1] = 0;

  for (i = 0; A[i] != 0; i++) {
  }

  __VERIFIER_assert(i <= N);
}
