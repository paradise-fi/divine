/* TAGS: sym inf c big */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc */
#include <stdbool.h>
extern int __VERIFIER_nondet_int(void);
extern bool __VERIFIER_nondet_bool(void);
extern void __VERIFIER_assert(int);

//x is an input variable
int x;

void foo() {
  x--;
}

int main() {
  x=__VERIFIER_nondet_int();
  while (x > 0) {
    _Bool c = __VERIFIER_nondet_bool();
    if(c) foo();
    else foo();
  }
  __VERIFIER_assert(x<=0);
}



