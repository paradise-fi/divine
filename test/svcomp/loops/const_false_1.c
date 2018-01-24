/* TAGS: c */
/* VERIFY_OPTS: --sequential -o nofail:malloc */
extern void __VERIFIER_error() __attribute__ ((__noreturn__));
extern void __VERIFIER_assert(int);


int main(void) {
  unsigned int x = 1;
  unsigned int y = 0;

  while (y < 1024) {
    x = 0;
    y++;
  }

  __VERIFIER_assert(x == 1); /* ERROR */
}
