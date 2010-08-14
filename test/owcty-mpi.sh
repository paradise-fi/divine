set -vex -o pipefail
not () { "$@" && exit 1 || return 0; }

# TODO there are some issues with counterexamples and MPI, apparently
mpiowcty() {
    $MPIEXEC -H localhost,localhost divine owcty --report "$@" 2> progress | tee report
}

check_valid() {
    grep "^Finished: Yes" report
    grep "^LTL-Property-Holds: Yes" report
}

check_invalid() {
    grep "^Finished: Yes" report
    grep "^LTL-Property-Holds: No" report
}

test -x "`which $MPIEXEC`" || exit 200
trap "cat progress" EXIT

mpiowcty peterson-naive.dve
check_invalid

grep "^Finished: Yes" report
grep "^LTL-Property-Holds: No" report

if ! grep -q "MAP/ET" progress; then
    grep '|S| = ' progress | sed -r -e 's,[^0-9]*([0-9]+).*,\1,' > numbers
    cat > numbers-right <<EOF
66566
66566
47598
47598
47598
EOF
    diff -u numbers-right numbers
fi

mpiowcty peterson-liveness.dve
check_valid

grep '|S| = ' progress | sed -r -e 's,[^0-9]*([0-9]+).*,\1,' > numbers
cat > numbers-right <<EOF
68642
380170
37595
201017
20693
120886
9117
61191
4649
29143
4070
25393
3565
21159
2915
17167
1304
7913
0
EOF
diff -u numbers-right numbers

for t in 1 4 5 6; do
    mpiowcty test$t.dve
    check_invalid
done

for t in 2 3; do
    mpiowcty test$t.dve
    check_valid
done
