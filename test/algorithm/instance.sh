. lib

if [ "$GEN_DVE" = "ON" ]; then
    testcase="data/empty.dve"
elif [ "$GEN_DUMMY" = "ON" ]; then
    testcase="--dummy"
else
    skip
fi

checkInstance() {
    key=$1
    msg=$2
    shift
    shift

    inst=`divine verify -p deadlock "$testcase" "$@" -r | grep 'Instance:'`
    if echo $inst | egrep -i "$key"; then
        echo "#### PASSED"
    else
        echo "ERROR: Missing $key in instance $msg (found $inst)"
        exit 1
    fi
}

# partitioned
checkInstance '\<Reachability.*Partitioned' "for reachability" --reachability
test "$ALG_WEAKREACHABILITY" = "ON" && checkInstance 'WeakReachability.*Partitioned' "for weak reachability" --weak
checkInstance 'Owcty.*Partitioned' "for OWCTY" --owcty
test "$ALG_MAP" = "ON" && checkInstance 'Map.*Partitioned' "for MAP" --map
test "$ALG_NDFS" = "ON" && checkInstance 'NestedDfs.*Partitioned' "for NestedDFS" --nested-dfs

#shared
checkInstance '\<Reachability.*Shared' "for shared reachability" --reachability --shared
test "$ALG_WEAKREACHABILITY" = "ON" && checkInstance 'WeakReachability.*Shared' "for shared weak reachability" --weak --shared
checkInstance 'Owcty.*Shared' "for shared OWCTY" --owcty --shared
test "$ALG_MAP" = "ON" && checkInstance 'Map.*Shared' "for shared MAP" --map --shared
test "$ALG_NDFS" = "ON" && checkInstance 'NestedDFS.*Shared' "for double threaded NestedDFS" --nested-dfs -w 2

if test "$STORE_COMPRESS" = "ON"; then
    checkInstance 'NTree' "with enabled compression" --compression=tree
fi
