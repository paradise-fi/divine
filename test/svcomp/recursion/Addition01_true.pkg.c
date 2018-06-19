/* VERIFY_OPTS: --symbolic --sequential */
/* TAGS: sym c */
/* CC_OPTS: */

extern void __VERIFIER_error() __attribute__ ((__noreturn__));

/*
 * Recursive implementation integer addition.
 *
 * Author: Matthias Heizmann
 * Date: 2013-07-13
 *
 */

// V: small.10 CC_OPT: -DNUM=10
// V: small.100 CC_OPT: -DNUM=100 TAGS: big
// V: small.1000 CC_OPT: -DNUM=1000 TAGS: big
// V: big.1073741823 CC_OPT: -DNUM=1073741823 TAGS: big

extern int __VERIFIER_nondet_int(void);

int addition(int m, int n) {
    if (n == 0) {
        return m;
    }
    else if (n > 0) {
        return addition(m+1, n-1);
    }
    else {
        return addition(m-1, n+1);
    }
}


int main() {
    int m = __VERIFIER_nondet_int();
    if (m < 0 || m > NUM) {
        // additional branch to avoid undefined behavior
        // (because of signed integer overflow)
        return 0;
    }
    int n = __VERIFIER_nondet_int();
    if (n < 0 || n > NUM) {
        // additional branch to avoid undefined behavior
        // (because of signed integer overflow)
        return 0;
    }
    int result = addition(m,n);
    if (result == m + n) {
        return 0;
    } else {
        ERROR: __VERIFIER_error();
    }
}
