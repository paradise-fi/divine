# TAGS:
. lib/testcase

cat > file.c <<EOF
int main( int argc, char **argv ) {
    argv[ argc + 1 ] = 0;
}
EOF

sim file.c <<EOF
+ ^# executing __boot at
> start
> setup --debug-everything
- ^# executing
+ ^# executing main at
> info registers
+ ^\s*Constants
+ ^\s*Globals
+ ^\s*Frame
+ ^\s*PC
> stepa
+ ^# executing __dios::FaultBase
> info registers
+ ^\s*Flags:.*IgnoreCrit
> info registers
+ ^\s*Flags:.*Error
> info registers
+ ^\s*Flags:.*KernelMode
EOF
