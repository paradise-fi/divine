# TAGS: min
. lib/testcase

cat > file.cpp <<EOF
int main() {
    int x = 42; // alloca should be removed
    int y = 0; // alloca should not be removed
    if ( x >= 0 )
        y = 1;
}
EOF

sim file.cpp <<EOF
+ ^# executing __boot at
> start
> step --count 3
+ ^# executing main at
> show .x
+ ^attributes
+ type:\s*int
+ value:\s*[i32 42 d]
> show .y
+ ^attributes
+ type:\s*int
EOF
