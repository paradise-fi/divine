set -vex
not () { "$@" && exit 1 || return 0; }

rm -f peterson-naive.dve.so
rm -f leader_election.dve.so
rm -f shuffle.dve.so
rm -f haoEEDM.dve.so

divine compile peterson-naive.dve
divine metrics --report peterson-naive.dve.so > report
grep "^Finished: Yes" report
grep "^States-Visited:" report
grep "^States-Visited: 94062" report

divine compile shuffle.dve
# test reachability as well, which has additional per-state data
divine reachability --report shuffle.dve.so > report
grep "^Finished: Yes" report
grep "^States-Visited:" report
grep "^States-Visited: 181450" report

divine compile leader_election.dve
divine metrics --report leader_election.dve.so > report
grep "^Finished: Yes" report
grep "^States-Visited:" report
grep "^States-Visited: 2152" report

divine reachability --report leader_election.dve.so > report 2> progress
grep FOUND progress
grep "^Finished: Yes" report

divine compile haoEEDM.dve
divine metrics --report haoEEDM.dve.so > report
grep "^States-Visited: 502" report
