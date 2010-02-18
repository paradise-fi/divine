set -vex
not() { "$@" && exit 1 || return 0; }
ndfs() { divine nested-dfs --report "$@" > report; }

check_valid() {
    grep "^Finished: Yes" report
    grep "^LTL-Property-Holds: Yes" report
}

check_invalid() {
    grep "^Finished: Yes" report
    grep "^LTL-Property-Holds: No" report
}

for t in 1 4 5 6; do
    ndfs test$t.dve
    check_invalid
done

for t in 2 3; do
    ndfs test$t.dve
    check_valid
done
