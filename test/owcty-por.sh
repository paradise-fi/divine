set -vex -o pipefail
trap "cat progress" EXIT
not () { "$@" && exit 1 || return 0; }

check_valid() {
    grep "^Finished: Yes" report
    grep "^LTL-Property-Holds: Yes" report
}

check_invalid() {
    grep "^Finished: Yes" report
    grep "^LTL-Property-Holds: No" report
}

divine owcty --por --report peterson-naive.dve 2> progress | tee report
check_invalid

divine owcty --por --report peterson-liveness.dve 2> progress | tee report
check_valid

for t in 1 4 5 6; do
    divine owcty --por --report test$t.dve > report 2> progress
    check_invalid
done

for t in 2 3; do
    divine owcty --por --report test$t.dve > report 2> progress
    check_valid
done
