/* TAGS: c sym */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: small.5 CC_OPT: -DNUM=5
// V: small.10 CC_OPT: -DNUM=10 TAGS: big
// V: big.100 CC_OPT: -DNUM=100 TAGS: big
// V: big.1000 CC_OPT: -DNUM=1000 TAGS: big
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

int main()
{
    int scheme;
    int urilen,tokenlen;
    int cp,c;
    urilen = __VERIFIER_nondet_int();
    tokenlen = __VERIFIER_nondet_int();
    scheme = __VERIFIER_nondet_int();
    if (!(urilen <= NUM && urilen >= -NUM)) return 0;
    if (!(tokenlen <= NUM && tokenlen >= -NUM)) return 0;
    if (!(scheme <= NUM && scheme >= -NUM)) return 0;

    if(urilen>0); else goto END;
    if(tokenlen>0); else goto END;
    if(scheme >= 0 );else goto END;
    if (scheme == 0 || (urilen-1 < scheme)) {
        goto END;
    }

    cp = scheme;

    __VERIFIER_assert(cp-1 < urilen);
    __VERIFIER_assert(0 <= cp-1);

    if (__VERIFIER_nondet_int()) {
        __VERIFIER_assert(cp < urilen);
        __VERIFIER_assert(0 <= cp);
        while ( cp != urilen-1) {
            if(__VERIFIER_nondet_int()) break;
            __VERIFIER_assert(cp < urilen);
            __VERIFIER_assert(0 <= cp);
            ++cp;
        }
        __VERIFIER_assert(cp < urilen);
        __VERIFIER_assert( 0 <= cp );
        if (cp == urilen-1) goto END;
        __VERIFIER_assert(cp+1 < urilen);
        __VERIFIER_assert( 0 <= cp+1 );
        if (cp+1 == urilen-1) goto END;
        ++cp;

        scheme = cp;

        if (__VERIFIER_nondet_int()) {
            c = 0;
            __VERIFIER_assert(cp < urilen);
            __VERIFIER_assert(0<=cp);
            while ( cp != urilen-1
                    && c < tokenlen - 1) {
                __VERIFIER_assert(cp < urilen);
                __VERIFIER_assert(0<=cp);
                if (__VERIFIER_nondet_int()) {
                    ++c;
                    __VERIFIER_assert(c < tokenlen);
                    __VERIFIER_assert(0<=c);
                    __VERIFIER_assert(cp < urilen);
                    __VERIFIER_assert(0<=cp);
                }
                ++cp;
            }
            goto END;
        }
    }

END:
    return 0;
}
