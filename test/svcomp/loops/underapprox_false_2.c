/* TAGS: c sym */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc */
extern void __VERIFIER_error() __attribute__ ((__noreturn__));
extern void __VERIFIER_assert(int);


int main(void) {
  unsigned int x = 0;
  unsigned int y = 1;

  while (x < 6) {
    x++;
    y *= 2;
  }

  __VERIFIER_assert(x != 6); /* ERROR */
}
