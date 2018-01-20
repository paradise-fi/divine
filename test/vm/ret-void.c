/* TAGS: min c */
#include <assert.h>

typedef void (*VoidFun)();

int intFun() { return 0; }

int main() {
    ((VoidFun)intFun)(); /* ERROR */
}
