/* VERIFY_OPTS: --symbolic --sequential */
/* TAGS: sym c */
#include <stdbool.h>
extern void __VERIFIER_assume(int);
extern void __VERIFIER_assert(int);
extern int __VERIFIER_nondet_int(void);

int main() {
  int i=0, x=0, y=0;
  int n=__VERIFIER_nondet_int();
  __VERIFIER_assume(n>0);
  for(i=0; i<n; i++)
  {
    x = x-y;
    __VERIFIER_assert(x==0);
    y = __VERIFIER_nondet_int();
    __VERIFIER_assume(y!=0);
    x = x+y;
    __VERIFIER_assert(x!=0);
  }
  __VERIFIER_assert(x==0); /* ERROR */
}

