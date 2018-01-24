/* TAGS: c */
/* VERIFY_OPTS: --sequential -o nofail:malloc */
/* CC_OPTS: -O2 */ // we cheat and let clang figure this one out
extern void __VERIFIER_error() __attribute__ ((__noreturn__));
extern void __VERIFIER_assert(int);


int main(void) {
  unsigned int x = 0;
  unsigned short N;

  while (x < N) {
    x += 2;
  }

  __VERIFIER_assert(x % 2); /* ERROR */
}
