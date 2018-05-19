/* VERIFY_OPTS: --symbolic --sequential */
/* TAGS: big inf sym c */
#define a (2)

#include <assert.h>
#include <stdbool.h>
extern unsigned int __VERIFIER_nondet_uint(void);

int main() {
  int sn=0;
  unsigned int loop1=__VERIFIER_nondet_uint(), n1=__VERIFIER_nondet_uint();
  unsigned int x=0;

  while(1){
    sn = sn + a;
    x++;
    assert(sn==x*a || sn == 0);
  }
}

