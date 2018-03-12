# TAGS:
. lib/testcase

cat > file.cpp <<EOF
#include <dios/sys/kernel.hpp>

extern "C" void use_choice()
{
    __vm_choose( 2 );
}

int main() {}
EOF

sim -std=c++14 -C,-I$SRC_ROOT/bricks file.cpp <<EOF
+ ^# executing __boot at
> start
> call use_choice
+ fault in debug mode
EOF
