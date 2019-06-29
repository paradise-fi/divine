# TAGS:
. lib/testcase

cat > prog.c <<EOF
int main()
{}
EOF

not dioscc prog.c -lxxx 2>&1 | tee file.txt

# lld | ld  error message
egrep "unable to find library -lxxx|cannot find -lxxx" file.txt
egrep "lld failed, not linked|failed to link, ld exited with exitcode" file.txt

test ! -s a.out
