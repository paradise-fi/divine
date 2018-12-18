/* TAGS: sym c */
/* VERIFY_OPTS: --symbolic */

#include <assert.h>

int __VERIFIER_nondet_int(); /* note the absence of void in the arg list */
int main()
{
    assert( __VERIFIER_nondet_int() != 3 ); /* ERROR */
}
