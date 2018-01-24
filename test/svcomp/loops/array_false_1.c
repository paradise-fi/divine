/* TAGS: c */
/* VERIFY_OPTS: --sequential -o nofail:malloc */
extern void __VERIFIER_error() __attribute__ ((__noreturn__));
extern void __VERIFIER_assert(int);


int main(void) {
  int A[2048];
  int i;

  for (i = 0; i < 1024; i++) {
    A[i] = i;
  }

  __VERIFIER_assert(A[1023] != 1023); /* ERROR */
}
