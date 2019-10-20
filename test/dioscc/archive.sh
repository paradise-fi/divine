# TAGS:
. lib/testcase

cat > f.c <<EOF
int f(int i)
{
    return i + 2;
}
EOF

cat > prog.c <<EOF
#include <assert.h>

int main()
{
    int i = 5;
    assert(f(i) != 7); /* ERROR */
    return 0;
}
EOF

dioscc -g -c f.c prog.c    # create object files

if ! [ -s f.o ] || ! [ -s prog.o ];   # make sure they exist
    then false;
fi

ar cr f.a f.o          # put function f into archive

if ! [ -s f.a ];
    then false;
fi

dioscc prog.o f.a        # fully compile

if [ -s linked.bc ];    # should NOT spawn linked.bc
    then false;
fi

if ! [ -s a.out ] || ! objdump -h a.out | grep .llvmbc;  # spawns executable w/ bitcode
    then false;
fi

{ echo expect --result error --location prog.c:6 ; echo verify a.out ; } > script
divcheck script
