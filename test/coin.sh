set -vex
not () { "$@" && exit 1 || return 0; }

divine metrics --report coffee.coin > report
grep "^Finished: Yes" report
grep "^States-Visited:" report
grep "^States-Visited: 7" report
