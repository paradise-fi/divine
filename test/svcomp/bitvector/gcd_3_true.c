/* VERIFY_OPTS: --symbolic --sequential -o ignore:integer */
/* TAGS: sym c big */

extern void __VERIFIER_assert(int);
extern char __VERIFIER_nondet_char(void);

signed char gcd_test(signed char a, signed char b)
{
    signed char t;

    if (a < (signed char)0) a = -a;
    if (b < (signed char)0) b = -b;

    while (b != (signed char)0) {
        t = b;
        b = a % b;
        a = t;
    }
    return a;
}


int main()
{
    signed char x = __VERIFIER_nondet_char();
    signed char y = __VERIFIER_nondet_char();
    signed char g;

    g = gcd_test(x, y);

    if (x > (signed char)0) {
        __VERIFIER_assert(x >= g);
    }

    return 0;
}
