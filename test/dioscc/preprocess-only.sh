# TAGS:
. lib/testcase

cat > prog.c <<EOF
int main(){}
EOF

dioscc -E prog.c | grep main

if [ -s a.out ] || [ -s prog.o ];
    then false;
fi
