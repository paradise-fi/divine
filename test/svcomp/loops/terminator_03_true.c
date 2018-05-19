/* VERIFY_OPTS: --symbolic --sequential */
/* TAGS: sym fin todo c big */
#include <stdbool.h>
#include <assert.h>
extern int __VERIFIER_nondet_int(void);
extern bool __VERIFIER_nondet_bool(void);

int main()
{
  int x=__VERIFIER_nondet_int();
  int y=__VERIFIER_nondet_int();

  if (y>0) /* guards against infinite behaviour */
  {
    while(x<100)
    {
      x=x+y;
     }
  }

  assert(y<=0 || (y>0 && x>=100));
}


