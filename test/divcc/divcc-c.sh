# TAGS: todo
. lib/testcase

cat > a.c <<EOF
int main(){}
EOF

cat > b.c <<EOF
int foo(){}
EOF

divcc -c a.c b.c

if ! [ -s a.c ] || ! [ -s b.c ];
    then false;
fi

if [ -s a.out ] || ! [ -s a.o ] || ! [ -s b.o ];
    then false;
fi

if ! objdump -h a.o | grep .llvmbc || ! objdump -h b.o | grep .llvmbc;
    then false;
fi
