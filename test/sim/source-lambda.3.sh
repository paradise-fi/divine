. lib/testcase

cat > file.cpp <<EOF
template< typename F >
void foo( F ) { }

int main() {
    foo( []( int ) {
        return 42;
    } );
}
EOF

sim -std=c++14 file.cpp <<EOF
+ ^# executing __boot at
> start
- ^# executing
+ ^# executing main at
> source
+ ^\s*4\s*int main\(\) \{
+ ^>>\s*5\s*    foo\( \[\]\( int \) \{
+ ^\s*6\s*        return 42;
+ ^\s*7\s*    \} \);
+ ^\s*8\s*\}
EOF
