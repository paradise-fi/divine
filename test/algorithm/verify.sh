. lib

verify_dve() {
    test "$O_DVE" = "ON" || test "$O_LEGACY" = "ON" || return 0

    check clear
    run verify data/assert.dve --property=assert
    check report Algorithm Reachability
    check report Property-Type goal
    check reachability_goal

    check clear
    run verify data/test1.dve --property=LTL
    check report Algorithm OWCTY
    check report Property-Type neverclaim
    check ltl_invalid

    check clear
    run verify data/test1.dve -w 1 --property=LTL
    check report Algorithm "Nested DFS"
    check report Property-Type neverclaim
    check ltl_invalid
}

verify_timed() {
    test "$O_TIMED" = "ON" || return 0

    check clear
    run verify data/bridge.xml --property=-1
    check report Algorithm Reachability
    check report Property-Type deadlock
	check reachability_valid

    check clear
    run verify data/bridge.xml --property=0 -w 1
    check report Algorithm "Nested DFS"
    check report Property-Type neverclaim
    check ltl_valid

    check clear
    run verify data/bridge.xml --property=1
    check report Algorithm OWCTY
    check report Property-Type neverclaim
    check ltl_invalid
}

verify_dve
verify_timed
