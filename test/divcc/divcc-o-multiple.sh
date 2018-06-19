# TAGS: todo
. lib/testcase

cat > prog.c <<EOF
int main(){}
EOF

cat > empty.c <<EOF
// nothing
EOF

divcc -o name prog.c empty.c

if [ -s a.out ] || ! [ -s name ] || ! [ -x name ];
    then false;
fi

if [ -s prog.o ] || [ -s empty.o ];   # no object file was spawned
    then false;
fi
