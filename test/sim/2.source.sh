. lib/testcase

cat > file.cpp <<EOF
int main() {
    struct X {
        ~X() { }
    } _;
}
EOF

sim file.cpp <<EOF
+ ^# executing __boot at
> source
> start
- ^# executing
+ ^# executing main at
> source
+ ^\s*1\s*int main\(\) \{
+ ^\s*2\s*    struct X \{
+ ^\s*3\s*        ~X\(\) \{ \}
+ ^\s*4\s*    \} _;
+ ^>>\s*5\s*\}
EOF
