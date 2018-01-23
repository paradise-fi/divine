/* VERIFY_OPTS: --symbolic --sequential */
/* TAGS: sym c */

#include <stdbool.h>
extern void __VERIFIER_assert(int);
extern int __VERIFIER_nondet_int(void);

int main()
{
  int x=__VERIFIER_nondet_int();
  int *p = &x;

  while(x<100) {
   (*p)++;
  }
  __VERIFIER_assert(0); /* ERROR */
}

