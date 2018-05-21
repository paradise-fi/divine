/* TAGS: c sym */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: small.10 CC_OPT: -DNUM=10
// V: small.100 CC_OPT: -DNUM=100
// V: big.1000 CC_OPT: -DNUM=1000 TAGS: ext
// V: big.10000 CC_OPT: -DNUM=10000 TAGS: big
// V: big.100000 CC_OPT: -DNUM=100000 TAGS: big

extern void __VERIFIER_error(void);
extern void __VERIFIER_assume(int);
void __VERIFIER_assert(int cond) {
  if (!(cond)) {
    ERROR: __VERIFIER_error();
  }
  return;
}
int __VERIFIER_nondet_int();

int main()
{
  /* buf is treated as an array of unsigned 8-byte ints */
  //  u_int8_t *p;
  int p;
  int i;
  int leader_len;
  int bufsize;
  int bufsize_0;
  int ielen;

  leader_len = __VERIFIER_nondet_int();
  bufsize = __VERIFIER_nondet_int();
  ielen = __VERIFIER_nondet_int();
  if (!(leader_len < NUM)) return 0;
  if (!(bufsize < NUM)) return 0;
  if (!(ielen < NUM)) return 0;

  // copy the contents of leader into buf
  if(leader_len >0); else goto END;
  if(bufsize >0); else goto END;
  if(ielen >0); else goto END;

  if (bufsize < leader_len)
    goto END;

  p = 0;

  bufsize_0 = bufsize;
  bufsize -= leader_len;
  p += leader_len;

  /* This is the fix. */
  if (bufsize < 2*ielen)
    goto END;

  for (i = 0; i < ielen && bufsize > 2; i++) {
    __VERIFIER_assert(0<=p);
    __VERIFIER_assert(p+1<bufsize_0);
      //    *p = 'x';
      //    *(p+1) = 'x';
    p += 2;
  }

  // if we wrote all of ie[], say how many bytes written in total,
  // otherwise, claim we wrote nothing
  // return (i == ielen ? p - (u_int8_t *)buf : 0);
 END:
  ;
}

