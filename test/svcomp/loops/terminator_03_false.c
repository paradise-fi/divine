/* VERIFY_OPTS: --symbolic --sequential */
/* TAGS: sym c */
#include <stdbool.h>
extern int __VERIFIER_nondet_int(void);
extern void __VERIFIER_assert(int);

int main()
{
  int x=__VERIFIER_nondet_int();
  int y=__VERIFIER_nondet_int();

  if (y>0)
  {
    while(x<100)
    {
      x=x+y;
     }
  }
  __VERIFIER_assert(y<=0 || (y<0 && x>=100)); /* ERROR */
}


