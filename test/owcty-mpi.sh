set -vex
not () { "$@" && exit 1 || return 0; }

test -x "$MPIEXEC" || exit 200

$MPIEXEC -H localhost,localhost divine owcty --report peterson-naive.dve > report 2> progress

grep "^Finished: Yes" report
grep "^LTL-Property-Holds: No" report

grep '|S| = ' progress | sed -r -e 's,[^0-9]*([0-9]+).*,\1,' > numbers
cat > numbers-right <<EOF
66566
66566
47598
47598
47598
EOF

if !grep -q "MAP: cycle found" progress; then
    diff -u numbers-right numbers
fi

$MPIEXEC -H localhost,localhost divine owcty --report peterson-liveness.dve > report 2> progress

grep "^Finished: Yes" report
grep "^LTL-Property-Holds: Yes" report

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

