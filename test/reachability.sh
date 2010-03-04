set -vex
not () { "$@" && exit 1 || return 0; }

divine reachability --report shuffle.dve > report 2> progress
grep NOT progress
grep "^Deadlock-Count: 0" report
grep "^States-Visited: 181450" report

divine reachability --report peterson-naive.dve > report 2> progress
grep FOUND progress
grep "^Deadlock-Count:" report
not grep "^Deadlock-Count: 0" report
grep "^Finished: Yes" report
grep "^States-Visited:" report
