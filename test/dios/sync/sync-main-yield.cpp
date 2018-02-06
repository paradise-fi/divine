/* VERIFY_OPTS: -o config:synchronous */
/* TAGS: c++ */

#include <dios.h>
#include <sys/divm.h>
#include <cassert>

int main() {
    __dios_yield(); /* ERROR */
}