#include <dios.h>
#include <assert.h>

int main() {
    assert( __dios_hardware_concurrency() == 0 );
}
