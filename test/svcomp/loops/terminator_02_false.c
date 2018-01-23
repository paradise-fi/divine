/* VERIFY_OPTS: --symbolic --sequential */
/* TAGS: sym c */
#include <stdbool.h>
extern int __VERIFIER_nondet_int(void);
extern bool __VERIFIER_nondet_bool(void);
extern void __VERIFIER_assert(int);

int main()
{
  int x=__VERIFIER_nondet_int();
  int y=__VERIFIER_nondet_int();
  int z=__VERIFIER_nondet_int();

  while(x<100 && 100<z)
  {
    _Bool tmp=__VERIFIER_nondet_bool();
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

  __VERIFIER_assert(0); /* ERROR */
}


