set -vex
set -o pipefail
not () { "$@" && exit 1 || return 0; }

test -x "`which $MPIEXEC`" || exit 200

$MPIEXEC -H localhost,localhost divine reachability --report peterson-naive.dve | tee report
grep "^Finished: Yes" report
grep "^Error-State-Count: 0" report
grep "^States-Visited:" report
grep "^States-Visited: 94062" report
