. lib

verify_dve() {
    test "$GEN_DVE" = "ON" || return 0

    for COMP in $COMPRESSIONS
    do
        check clear
        run verify data/assert.dve --property=assert --compression=$COMP
        check report Algorithm Reachability
        check report Property-Type goal
        check reachability_goal

        check clear
        run verify data/test1.dve --property=LTL --compression=$COMP
        check report Algorithm OWCTY
        check report Property-Type neverclaim
        check ltl_invalid

        check clear
        run verify data/test1.dve -w 1 --property=LTL --compression=$COMP
        check report Algorithm "Nested DFS"
        check report Property-Type neverclaim
        check ltl_invalid
    done
}

verify_timed() {
    test "$GEN_TIMED" = "ON" || return 0

    for COMP in $COMPRESSIONS
    do
        check clear
        run verify data/bridge.xml --property=deadlock
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
    done
}

verify_dve
verify_timed
