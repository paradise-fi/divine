set -vex
not () { "$@" && exit 1 || return 0; }

rm -f sci.m.so
rm -f adash.dve.so
rm -f abp.m.so

check() {
divine metrics --report $1.so > report
grep "^Finished: Yes" report
grep "^States-Visited:" report
grep "^States-Visited: $2" report
grep "^Transition-Count: $3" report
divine reachability --report sci.m.so > report 2> progress
grep "^Finished: Yes" report
grep $4 progress

}

divine compile sci.m 2> err || {
    grep "does not support" err && exit 200
    cat err
    exit 1
}
check sci.m 18193 52186 NOT

divine compile adash.m
check adash.m 10466 35307 NOT

divine compile abp.m
check abp.m 80 144 NOT
