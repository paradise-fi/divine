/* VERIFY_OPTS: --symbolic --sequential */
/* TAGS: big inf sym c */
#define a (2)
#include <assert.h>
#include <stdbool.h>
extern int __VERIFIER_nondet_int(void);
int main() {
  int i, n=__VERIFIER_nondet_int(), sn=0;
  for(i=1; i<=n; i++) {
    sn = sn + a;
  }
  assert(sn==n*a || sn == 0);
}
