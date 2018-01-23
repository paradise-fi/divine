/* VERIFY_OPTS: --symbolic --sequential */
/* TAGS: sym todo c */
#include <stdbool.h>
extern void __VERIFIER_assert(int);
extern int __VERIFIER_nondet_int(void);

int main()
{
  int x,y;

  x=0;
  y=4;


  while(1)
    {
      x = x + y;
      y = y +4;


      __VERIFIER_assert(x!=30);
    }
}

