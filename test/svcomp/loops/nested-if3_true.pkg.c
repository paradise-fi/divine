/* TAGS: c sym big */
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
// V: big.100 CC_OPT: -DNUM=100
// V: big.1000 CC_OPT: -DNUM=1000
// V: big.10000 CC_OPT: -DNUM=10000
// V: big.100000 CC_OPT: -DNUM=100000

int main() {
  int i,k,n,l;

  n = __VERIFIER_nondet_int();
  l = __VERIFIER_nondet_int();
  if (!(l>0)) return 0;
  if (!(l < NUM)) return 0;
  if (!(n < NUM)) return 0;
  for (k=1;k<n;k++){
    for (i=l;i<n;i++){
      __VERIFIER_assert(1<=i);
    }
    if(__VERIFIER_nondet_int())
      l = l + 1;
  }
 }
