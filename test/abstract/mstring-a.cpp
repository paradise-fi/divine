/* TAGS: mstring min sym */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */

#include <rst/domains.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    char buff[] = "string";
    const char * str = __mstring_from_string( buff );
}
