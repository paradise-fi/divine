/* TAGS: sym inf c big */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc */
#include <stdbool.h>
extern unsigned int __VERIFIER_nondet_uint(void);
extern bool __VERIFIER_nondet_bool(void);
extern void __VERIFIER_assert(int);

void foo()
{
  int y=0;
  bool c1=__VERIFIER_nondet_bool(), c2=__VERIFIER_nondet_bool();
  if (c1) y++;
  if (c2) y--;
  else y+=10;
}

int main()
{
  int d = 1;
  int x;
  bool c1=__VERIFIER_nondet_bool(), c2=__VERIFIER_nondet_bool();

  if (c1) d = d - 1;
  if (c2) foo();

  c1=__VERIFIER_nondet_bool(), c2=__VERIFIER_nondet_bool();

  if (c1) foo();
  if (c2) d = d - 1;

  while(x>0)
  {
    x=x-d;
  }

  __VERIFIER_assert(x<=0);
}

