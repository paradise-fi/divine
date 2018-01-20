# TAGS: min
. lib/testcase

cat > a.c <<EOF
void dont_link_test_foo( void ) { }
EOF
cat > b.c <<EOF
void dont_link_test_bar( void ) { }
EOF

divine cc -c a.c b.c
llvm-nm a.bc | grep dont_link_test_foo
llvm-nm b.bc | grep dont_link_test_bar

if llvm-nm a.bc | grep dont_link_test_bar || llvm-nm b.bc | grep dont_link_test_foo; then
    false
fi
