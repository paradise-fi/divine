/* VERIFY_OPTS: --symbolic --sequential */
/* TAGS: sym c */
#define a (2)

#include <stdbool.h>
extern void __VERIFIER_assert(int);
extern unsigned int __VERIFIER_nondet_uint(void);
int main() {
  int i, n=__VERIFIER_nondet_uint(), sn=0;
  for(i=1; i<=n; i++) {
    sn = sn + a;
    if (i==4) sn=-10;
  }
  __VERIFIER_assert(sn==n*a || sn == 0); /* ERROR */
}
