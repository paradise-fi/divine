/* VERIFY_OPTS: --symbolic --sequential -o ignore:arithmetic */
/* TAGS: sym c */

/* NB. The ignore:arithmetic is a workaround for --symbolic also running a
 * concrete computation (which does not follow path constraints), in which the
 * divisor happens to be zero... */

#include <stdbool.h>
extern void __VERIFIER_assert(int);
signed char gcd_test(signed char a, signed char b)
{
    signed char t;

    if (a < 0) a = -a;
    if (b < 0) b = -b;

    while (b != 0) {
        t = b;
        b = a % b;
        a = t;
    }
    return a;
}


extern char __VERIFIER_nondet_char(void);

int main()
{
    signed char x = __VERIFIER_nondet_char();
    signed char y = __VERIFIER_nondet_char();
    signed char g;

    if (y > 0 && x % y == 0) {
        g = gcd_test(x, y);

        __VERIFIER_assert(g == y);
    }

    return 0;
}
