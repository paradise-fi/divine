# TAGS: min
. lib/testcase

cat > prog.c <<EOF
int main(){}
EOF

dioscc prog.c

if ! [ -s a.out ] || ! [ -x a.out ];   # a.out exists, is non-empty and executable
    then false;
fi

if [ -s prog.o ];   # and no object file was spawned
    then false;
fi
