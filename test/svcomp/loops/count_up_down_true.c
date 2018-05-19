/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc */
/* TAGS: big inf sym c */
#include <stdbool.h>
extern unsigned int __VERIFIER_nondet_uint(void);
extern void __VERIFIER_assert(int);

int main()
{
  unsigned int n = __VERIFIER_nondet_uint();
  unsigned int x=n, y=0;
  while(x>0)
  {
    x--;
    y++;
  }
  __VERIFIER_assert(y==n);
}

