/* VERIFY_OPTS: --symbolic --sequential */
/* TAGS: sym todo c */
#include <stdbool.h>
extern void __VERIFIER_assert(int);
extern int __VERIFIER_nondet_int(void);

int main()
{
  int y = __VERIFIER_nondet_int();

  y = 1;

  while(1)
    {
      y = y +2*__VERIFIER_nondet_int();


      __VERIFIER_assert(y!=0);

    }
}

