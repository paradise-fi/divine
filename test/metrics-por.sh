set -vex
not () { "$@" && exit 1 || return 0; }

divine metrics --por --report peterson-naive.dve > report
grep "^Finished: Yes" report
grep "^States-Visited:" report > states
test `cut -f2 -d: < states` -le 94062

divine metrics --report leader_election.dve > report
grep "^Finished: Yes" report
grep "^States-Visited:" report > states
test `cut -f2 -d: < states` = 2152

divine metrics --por --report leader_election.dve > report
grep "^Finished: Yes" report
grep "^States-Visited:" report > states
test `cut -f2 -d: < states` -lt 2152

