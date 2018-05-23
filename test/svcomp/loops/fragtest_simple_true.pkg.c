/* TAGS: c sym */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

extern void __VERIFIER_error(void);
extern void __VERIFIER_assume(int);
void __VERIFIER_assert(int cond) {
  if (!(cond)) {
    ERROR: __VERIFIER_error();
  }
  return;
}
extern int __VERIFIER_nondet_int(void);
// V: small.10 CC_OPT: -DNUM=10
// V: small.100 CC_OPT: -DNUM=100 TAGS: big
// V: big.1000 CC_OPT: -DNUM=1000 TAGS: big
// V: big.10000 CC_OPT: -DNUM=10000 TAGS: big
// V: big.100000 CC_OPT: -DNUM=100000 TAGS: big

int main(){
  int i,pvlen ;
  int tmp___1 ;
  int k = 0;
  int n;

  i = 0;
  pvlen = __VERIFIER_nondet_int();

  //  pkt = pktq->tqh_first;
  while ( __VERIFIER_nondet_int() && i <= NUM) {
    i = i + 1;
  }


  if (i > pvlen) {
    pvlen = i;
  }
  i = 0;

  while ( __VERIFIER_nondet_int() && i <= NUM) {
    tmp___1 = i;
    i = i + 1;
    k = k + 1;
  }

  int j = 0;
  n = i;
  while (1) {

    __VERIFIER_assert(k >= 0);
    k = k -1;
    i = i - 1;
    j = j + 1;
    if (j >= n) {
      break;
    }
  }
  return 0;

}
