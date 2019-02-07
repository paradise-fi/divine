# TAGS:
. lib/testcase

cat > prog.c <<EOF
int main()
{}
EOF

not divcc prog.c -lxxx 2>&1 | tee file.txt

grep "unable to find library -lxxx" file.txt
grep "lld failed, not linked" file.txt

test ! -s a.out
