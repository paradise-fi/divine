# TAGS: divcc dioscc min
. lib/testcase

cat > a.c <<EOF
int main(){}
EOF

for COMPILER in clang clang++ dioscc diosc++ divcc divc++
do
	$COMPILER a.c
    file a.out | grep "dynamically linked"
    rm a.out

    $COMPILER a.c --static
    file a.out | grep "statically linked"
    rm a.out

    $COMPILER a.c -static
    file a.out | grep "statically linked"
    rm a.out
done
