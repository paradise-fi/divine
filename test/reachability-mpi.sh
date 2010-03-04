set -vex
set -o pipefail
not () { "$@" && exit 1 || return 0; }

test -x "`which $MPIEXEC`" || exit 200

$MPIEXEC -H localhost,localhost divine reachability --report shuffle.dve | tee report
grep "^Finished: Yes" report
grep "^States-Visited:" report
grep "^States-Visited: 181450" report
