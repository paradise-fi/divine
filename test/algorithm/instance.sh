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

    inst=`divine verify "$testcase" "$@" -r | grep 'Instance:'`
    if echo $inst | egrep -i "$key"; then
        echo "#### PASSED"
    else
        echo "ERROR: Missing $key in instance $msg (found $inst)"
        exit 1
    fi
}

# partitioned
checkInstance '\<Reachability.*Partitioned' "for reachability" --reachability
checkInstance 'WeakReachability.*Partitioned' "for weak reachability" --weak
checkInstance 'Owcty.*Partitioned' "for OWCTY" --owcty
checkInstance 'Map.*Partitioned' "for MAP" --map
checkInstance 'NestedDfs.*Partitioned' "for NestedDFS" --nested-dfs

#shared
checkInstance '\<Reachability.*Shared' "for shared reachability" --reachability --shared
checkInstance 'WeakReachability.*Shared' "for shared weak reachability" --weak --shared
checkInstance 'Owcty.*Shared' "for shared OWCTY" --owcty --shared
checkInstance 'Map.*Shared' "for shared MAP" --map --shared
checkInstance 'NestedDFS.*Shared' "for double threaded NestedDFS" --nested-dfs -w 2

if test "$STORE_COMPRESS" = "ON"; then
    checkInstance 'NTree' "with enabled compression" --compression=tree
fi
