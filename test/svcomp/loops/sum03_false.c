/* VERIFY_OPTS: --symbolic --sequential */
/* TAGS: sym c */
#define a (2)

#include <stdbool.h>
extern void __VERIFIER_assert(int);
extern unsigned int __VERIFIER_nondet_uint(void);

int main() {
  int sn=0;
  unsigned int loop1=__VERIFIER_nondet_uint(), n1=__VERIFIER_nondet_uint();
  unsigned int x=0;

  while(1){
    if (x<10)
      sn = sn + a;
    x++;
    __VERIFIER_assert(sn==x*a || sn == 0); /* ERROR */
  }
}

