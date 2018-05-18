/* VERIFY_OPTS: --symbolic --sequential */
/* TAGS: sym big c */
#include <stdbool.h>
extern void __VERIFIER_assert(int);
extern unsigned int __VERIFIER_nondet_uint(void);

int main() {
  unsigned int i, n=__VERIFIER_nondet_uint(), sn=0;
  for(i=0; i<=n; i++) {
    sn = sn + i;
  }
  __VERIFIER_assert(sn==(n*(n+1))/2 || sn == 0);
}
