/* TAGS: c sym todo big inf */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc */

/* Variant: This one just blindly copies the input into buffer and writes '>''\0' at the end. */

extern void __VERIFIER_error(void);
extern void __VERIFIER_assume(int);
void __VERIFIER_assert(int cond) {
  if (!(cond)) {
    ERROR: __VERIFIER_error();
  }
  return;
}
int __VERIFIER_nondet_int();
#define LARGE_INT 1000000

int main ()
{
  int in;
  int inlen = __VERIFIER_nondet_int();
  int bufferlen = __VERIFIER_nondet_int();
  int buf;
  int buflim;

  if(bufferlen >1);else goto END;
  if(inlen > 0);else goto END;
  if(bufferlen < inlen);else goto END;

  buf = 0;
  in = 0;
  buflim = bufferlen - 2;

  while (__VERIFIER_nondet_int())
  {
    if (buf == buflim)
      break;
    __VERIFIER_assert(0<=buf);
    __VERIFIER_assert(buf<bufferlen);
    buf++;
out:
    in++;
    __VERIFIER_assert(0<=in);
    __VERIFIER_assert(in<inlen);
  }

    __VERIFIER_assert(0<=buf);
    __VERIFIER_assert(buf<bufferlen);
    buf++;

  /* OK */
  __VERIFIER_assert(0<=buf);//6
  __VERIFIER_assert(buf<bufferlen);

  buf++;

 END:  return 0;
}
