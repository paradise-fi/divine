/* VERIFY_OPTS: --symbolic --sequential */
/* TAGS: sym c */
#include <stdbool.h>
extern void __VERIFIER_assert(int);
extern void __VERIFIER_assume(int);
extern int __VERIFIER_nondet_int(void);
extern bool __VERIFIER_nondet_bool(void);

int main()
{
  int x=__VERIFIER_nondet_int();
  int y=__VERIFIER_nondet_int();
  int z=__VERIFIER_nondet_int();
  __VERIFIER_assume(x<100);
  __VERIFIER_assume(z<100);
  while(x<100 && 100<z)
  {
    bool tmp=__VERIFIER_nondet_bool();
    if (tmp)
   {
     x++;
   }
   else
   {
     x--;
     z--;
   }
  }

  __VERIFIER_assert(x>=100 || z<=100);
}


