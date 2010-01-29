set -vex
not () { "$@" && exit 1 || return 0; }

test -f ../examples/BenchmarkHs.so || exit 200

divine metrics --report ../examples/BenchmarkHs.so > report
grep "^Finished: Yes" report
grep "^Error-State-Count: 0" report
grep "^States-Visited:" report
grep "^States-Visited: 16640" report

divine reachability --report ../examples/BenchmarkHs.so > report
grep "^Finished: Yes" report
grep "^Error-State-Count: 0" report
grep "^States-Visited:" report
grep "^States-Visited: 16640" report
