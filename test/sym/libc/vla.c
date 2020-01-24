/* TAGS: sym c min todo */
/* VERIFY_OPTS: --symbolic */

#include <sys/lamp.h>
#include <assert.h>

int main()
{
    unsigned size = __lamp_any_i32();
    int vla[size];
}
