// VERIFY_OPTS: -o ncpus:42

#include <dios.h>
#include <assert.h>

int main() {
    assert( __dios_hardware_concurrency() == 42 );
}
