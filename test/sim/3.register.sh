. lib/testcase

cat > file.c <<EOF
int main( int argc, char **argv ) {
    argv[ argc + 1 ] = 0;
}
EOF

sim file.c <<EOF
+ ^# executing __boot at
> start
> setup --debug kernel
- ^# executing
+ ^# executing main at
> register
+ ^\s*Constants
+ ^\s*Globals
+ ^\s*Frame
+ ^\s*PC
> stepa
+ ^# executing \{Fault\}
> register
+ ^\s*Flags:.*Mask
> register
+ ^\s*Flags:.*Error
> register
+ ^\s*Flags:.*KernelMode
EOF
