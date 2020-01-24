/* TAGS: c sym min */
/* VERIFY_OPTS: --symbolic */

#include <stdbool.h>
#include <assert.h>

bool __VERIFIER_nondet_bool();

int main()
{
    bool a = __VERIFIER_nondet_bool();
    bool b = !a;

    assert( a == b ); /* ERROR */
}
