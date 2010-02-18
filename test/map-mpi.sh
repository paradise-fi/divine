set -vex -o pipefail
not () { "$@" && exit 1 || return 0; }

test -x "`which $MPIEXEC`" || exit 200

check_valid() {
    grep "^Finished: Yes" report
    grep "^LTL-Property-Holds: Yes" report
}

check_invalid() {
    grep "^Finished: Yes" report
    grep "^LTL-Property-Holds: No" report
}

mpimap() {
    $MPIEXEC -H localhost,localhost divine map --report "$@" | tee report
}

mpimap peterson-naive.dve
check_invalid

mpimap peterson-liveness.dve
check_valid

for t in 1 4 5 6; do
    mpimap test$t.dve
    check_invalid
done

for t in 2 3; do
    mpimap test$t.dve
    check_valid
done
