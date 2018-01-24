/* TAGS: c */
/* VERIFY_OPTS: --sequential -o nofail:malloc */
/* CC_OPTS: -O2 */ // we cheat and let clang figure this one out
extern void __VERIFIER_error() __attribute__ ((__noreturn__));
extern void __VERIFIER_assert(int);


unsigned int f(unsigned int z) {
  return z + 2;
}

int main(void) {
  unsigned int x = 0;

  while (x < 0x0fffffff) {
    x = f(x);
  }

  __VERIFIER_assert(x % 2); /* ERROR */
}
