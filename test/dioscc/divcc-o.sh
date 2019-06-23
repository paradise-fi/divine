# TAGS:
. lib/testcase

cat > prog.c <<EOF
int main(){}
EOF

dioscc -o name prog.c

if [ -s a.out ] || ! [ -s name ] || ! [ -x name ];
    then false;
fi

if [ -s prog.o ];   # no object file was spawned
    then false;
fi
