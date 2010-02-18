set -vex -o pipefail
not() { "$@" && exit 1 || return 0; }
map() { divine map --report "$@" | tee report; }

check_valid() {
    grep "^Finished: Yes" report
    grep "^LTL-Property-Holds: Yes" report
}

check_invalid() {
    grep "^Finished: Yes" report
    grep "^LTL-Property-Holds: No" report
}


map peterson-naive.dve
check_invalid

map peterson-liveness.dve
check_valid

for t in 1 4 5 6; do
    map test$t.dve
    check_invalid
done

for t in 2 3; do
    map test$t.dve
    check_valid
done
