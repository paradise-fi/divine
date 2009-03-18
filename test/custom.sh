set -vex
not () { "$@" && exit 1 || return 0; }

test -f ../examples/BenchmarkC.so || exit 200

divine metrics --report ../examples/BenchmarkC.so > report
grep "^Finished: Yes" report
grep "^Error-State-Count: 0" report
grep "^States-Visited:" report
grep "^States-Visited: 1050624" report
