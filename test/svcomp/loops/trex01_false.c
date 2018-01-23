/* VERIFY_OPTS: --symbolic --sequential -o ignore:control */
/* TAGS: sym c */
#include <stdbool.h>
extern int __VERIFIER_nondet_int(void);
extern bool __VERIFIER_nondet_bool(void);
extern void __VERIFIER_assert(int);

void f(int d) {
  int x, y, k, z = 1;
  L1:
  while (z < k) { z = 2 * z; }
  __VERIFIER_assert(z>=2); /* ERROR */
  L2:
  while (x > 0 && y > 0) {
    bool c = __VERIFIER_nondet_bool();
    if (c) {
      P1:
      x = x - d;
      y = __VERIFIER_nondet_bool();
      z = z - 1;
    } else {
      y = y - d;
    }
  }
}

void main() {
  bool c = __VERIFIER_nondet_bool();
  if (c) {
    f(1);
  } else {
    f(2);
  }
}


