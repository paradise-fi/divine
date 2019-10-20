# TAGS:
. lib/testcase

cat > file.cpp <<EOF
#include <sys/divm.h>

extern "C" void use_choice()
{
    __vm_choose( 2 );
}

int main() {}
EOF

sim -std=c++17 file.cpp <<EOF
+ ^# executing __boot at
> start
> call use_choice
+ __vm_choose is not allowed in debug mode
EOF
