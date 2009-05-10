set -vex
not () { "$@" && exit 1 || return 0; }

check_valid() {
    grep "^Finished: Yes" report
    grep "^LTL-Property-Holds: Yes" report
}

check_invalid() {
    grep "^Finished: Yes" report
    grep "^LTL-Property-Holds: No" report
}

divine nested-dfs --report test1.dve > report 2> progress
check_invalid

divine nested-dfs --report test2.dve > report 2> progress
check_valid

divine nested-dfs --report test3.dve > report 2> progress
check_valid

divine nested-dfs --report test4.dve > report 2> progress
check_invalid

divine nested-dfs --report test5.dve > report 2> progress
check_invalid

divine nested-dfs --report test6.dve > report 2> progress
check_invalid
