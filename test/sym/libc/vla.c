/* TAGS: sym c min todo */
/* VERIFY_OPTS: --symbolic */

#include <rst/domains.h>
#include <assert.h>

int main()
{
    unsigned size = __sym_val_i32();
    int vla[size];
}
