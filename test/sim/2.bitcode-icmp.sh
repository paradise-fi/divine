. lib/testcase

cat > file.cpp <<EOF
int main( int argc, char ** ) {
    return argc >= 0;
}
EOF

sim file.cpp <<EOF
+ ^# executing __boot at
> start
- ^# executing
+ ^# executing main at
> bitcode
+ .*dbg.declare
+ >>.*icmp\.sge.*\[i32 0 d]
+ ret
EOF
