/* VERIFY_OPTS: --symbolic --sequential */
/* TAGS: sym todo c */
#include <stdbool.h>
extern void __VERIFIER_assert(int);
extern int __VERIFIER_nondet_int(void);

int main()
{
  int x,y,z;

  x=0;
  y=0;
  z=0;

  while(1)
    {
      x = x +2*__VERIFIER_nondet_int();
      y = y +4*__VERIFIER_nondet_int();
      z = z +8*__VERIFIER_nondet_int();

      __VERIFIER_assert(4*x+2*y+z!=4);
    }
}

