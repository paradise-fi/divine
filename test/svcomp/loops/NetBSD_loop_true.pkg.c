/* TAGS: c sym */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: small.10 CC_OPT: -DNUM=10
// V: small.100 CC_OPT: -DNUM=100
// V: big.1000 CC_OPT: -DNUM=1000 TAGS: ext
// V: big.10000 CC_OPT: -DNUM=10000 TAGS: big
// V: big CC_OPT: -DNUM=2147483647 TAGS: big

extern void __VERIFIER_error(void);
extern void __VERIFIER_assume(int);
void __VERIFIER_assert(int cond) {
  if (!(cond)) {
    ERROR: __VERIFIER_error();
  }
  return;
}
int __VERIFIER_nondet_int();

int main ()
{
  int MAXPATHLEN;
  int pathbuf_off;

  /* Char *bound = pathbuf + sizeof(pathbuf)/sizeof(*pathbuf) - 1; */
  int bound_off;

  /* glob2's local vars */
  /* Char *p; */
  int glob2_p_off;
  int glob2_pathbuf_off;
  int glob2_pathlim_off;

  MAXPATHLEN = __VERIFIER_nondet_int();
  if(MAXPATHLEN > 0 && MAXPATHLEN < NUM); else goto END;

  pathbuf_off = 0;
  bound_off = pathbuf_off + (MAXPATHLEN + 1) - 1;


  glob2_pathbuf_off = pathbuf_off;
  glob2_pathlim_off = bound_off;

  for (glob2_p_off = glob2_pathbuf_off;
      glob2_p_off <= glob2_pathlim_off;
      glob2_p_off++) {
    /* OK */
    __VERIFIER_assert (0 <= glob2_p_off );
    __VERIFIER_assert (glob2_p_off < MAXPATHLEN + 1);
  }

 END:  return 0;
}
