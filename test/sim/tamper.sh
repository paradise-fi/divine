. lib/testcase

expect() {
    PATTERN_COUNT_EXP=$1
    PATTERN=$2
    FILE=$3
    PATTERN_COUNT=`grep -c "$PATTERN" $FILE`

    if [ $PATTERN_COUNT -ne $PATTERN_COUNT_EXP ] ; then
        check die "Expected $PATTERN_COUNT_EXP occurences of \"$PATTERN\", found $PATTERN_COUNT instead."
    fi
}

cat > file.c <<EOF
int main( int argc, char **argv ) {
    int x;
    if ( argc > 2 )
        x = argc;
    else
        x = 0;
    return x;
}
EOF

OUTF_PURE=pure.bc
OUTF=tamper.bc

sim file.c <<EOF
+ ^# executing __boot at
> bitcode --dump $OUTF_PURE
! ERROR
> start
+ ^# executing main at
> stepi
> stepi
> tamper .x
! ERROR
> bitcode --dump $OUTF
! ERROR
> q
EOF

not $BINDIR/llvm/bin/llvm-diff $OUTF_PURE $OUTF main 2> lldiff

expect 3 '%x\.abstract' lldiff
expect 2 '> *%x\.store' lldiff
expect 1 '> *store i1 true, i1\* %x\.fresh' lldiff
expect 2 '> *store i1 false, i1\* %x\.fresh' lldiff

divine verify --symbolic $OUTF
grep 'error found: yes' ${OUTF%%.bc}.report || check die "Verification should have failed"
