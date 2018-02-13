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

divcc -g -c f.c prog.c    # create object files

if ! [ -s f.o ] || ! [ -s prog.o ];   # make sure they exist
    then false;
fi

ar cr f.a f.o          # put function f into archive

if ! [ -s f.a ];
    then false;
fi

divcc prog.o f.a        # fully compile

if ! [ -s linked.bc ];  # should spawn linked.bc
    then false;
fi

divine verify linked.bc | tee verify.out
check verify prog.c
