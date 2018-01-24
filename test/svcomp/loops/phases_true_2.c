/* TAGS: c sym */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc */
extern void __VERIFIER_error() __attribute__ ((__noreturn__));
extern void __VERIFIER_assert(int);
extern void __VERIFIER_assume(int);
extern unsigned int __VERIFIER_nondet_uint(void);


int main(void) {
  unsigned int x = 1;
  unsigned int y = __VERIFIER_nondet_uint();

  if (!(y > 0)) return 0;

  while (x < y) {
    if (x < y / x) {
      x *= x;
    } else {
      x++;
    }
  }

  __VERIFIER_assert(x == y);
}
