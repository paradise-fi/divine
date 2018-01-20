# TAGS:
. lib/testcase

cat > prog.c <<EOF
int main(){}
EOF

divcc -E prog.c | grep main

if [ -s a.out ] || [ -s prog.o ];
    then false;
fi
