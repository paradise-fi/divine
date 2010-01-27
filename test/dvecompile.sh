set -vex
not () { "$@" && exit 1 || return 0; }

rm -f peterson-naive.dve.so
rm -f leader_election.dve.so

divine compile peterson-naive.dve
divine metrics --report peterson-naive.dve.so > report
grep "^Finished: Yes" report
grep "^Error-State-Count: 0" report
grep "^States-Visited:" report
grep "^States-Visited: 94062" report

# test reachability as well, which has additional per-state data
divine reachability --report peterson-naive.dve.so > report
grep "^Finished: Yes" report
grep "^Error-State-Count: 0" report
grep "^States-Visited:" report
grep "^States-Visited: 94062" report

divine compile leader_election.dve
divine reachability --report leader_election.dve.so > report
grep "^Finished: Yes" report
grep "^Error-State-Count: 0" report
grep "^States-Visited:" report
grep "^States-Visited: 2152" report
