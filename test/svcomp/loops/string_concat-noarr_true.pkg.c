/* TAGS: c sym */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc */
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

int main(void) {
  int i, j;

 L0:
  i = 0;
 L1:
  while( __VERIFIER_nondet_int() && i < NUM){

    i++;
  }
  if(i >= 100) STUCK: goto STUCK; //  assume( i < 100 );
  j = 0;
 L2:
  while( __VERIFIER_nondet_int() && i < NUM ){
    i++;
    j++;
  }
  if(j >= 100) goto STUCK; // assume( j < 100 );
  __VERIFIER_assert( i < 200 ); /* prove we don't overflow z */
  return 0;
}
