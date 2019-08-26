# TAGS:
. lib/testcase

cat > file.cpp <<EOF
#include <dios/sys/kernel.hpp>

extern "C" void use_debug()
{
    char buf[20];
    sprintf( buf, "indent = %d", __dios::get_debug().kernel_indent );
    __vm_trace( _VM_T_Text, buf );
}

int main() {}
EOF

sim -std=c++17 -C,-I$SRC_ROOT/bricks file.cpp <<EOF
+ ^# executing __boot at
> start
> call use_debug
- fault
+ ^  indent = 0
EOF
