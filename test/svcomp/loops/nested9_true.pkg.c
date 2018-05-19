/* TAGS: c sym */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc */
extern void __VERIFIER_error(void);
extern void __VERIFIER_assume(int);
void __VERIFIER_assert(int cond) {
  if (!(cond)) {
    ERROR: __VERIFIER_error();
  }
  return;
}
extern int __VERIFIER_nondet_int(void);

// V: small.5 CC_OPT: -DNUM=5
// V: small.10 CC_OPT: -DNUM=10 TAGS: ext
// V: big.100 CC_OPT: -DNUM=100 TAGS: big
// V: big.1000 CC_OPT: -DNUM=1000 TAGS: big
// V: big.10000 CC_OPT: -DNUM=10000 TAGS: big
// V: big.100000 CC_OPT: -DNUM=100000 TAGS: big

int main() {
    int i,j,k,n,l,m;

    n = __VERIFIER_nondet_int();
    m = __VERIFIER_nondet_int();
    l = __VERIFIER_nondet_int();
    if (!(-NUM < n && n < NUM)) return 0;
    if (!(-NUM < m && m < NUM)) return 0;
    if (!(-NUM < l && l < NUM)) return 0;
    if(3*n<=m+l); else goto END;
    for (i=0;i<n;i++)
        for (j = 2*i;j<3*i;j++)
            for (k = i; k< j; k++)
                __VERIFIER_assert( k-i <= 2*n );
END:
    return 0;
}
