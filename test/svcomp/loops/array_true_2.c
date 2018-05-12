/* TAGS: c todo */
/* VERIFY_OPTS: --sequential -o nofail:malloc */
extern void __VERIFIER_error() __attribute__ ((__noreturn__));
extern void __VERIFIER_assert(int);


#define SZ 2048

int main(void) {
  int A[SZ];
  int B[SZ];
  int i;
  int tmp;

  for (i = 0; i < SZ; i++) {
    tmp = A[i]; // A[i] is undefined
    B[i] = tmp;
  }

  __VERIFIER_assert(A[SZ/2] == B[SZ/2]);
}
