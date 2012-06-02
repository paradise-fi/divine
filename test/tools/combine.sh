set -vex
not () { "$@" && exit 1 || return 0; }

ltl_proc() {
    grep -A 1000 LTL_property $1
}

check_peterson1() {
    ltl_proc $1 | grep "q1 -> q2" | grep "guard P_0.CS";
    ltl_proc $1 | grep "q1 -> q1" | grep "{}";
    ltl_proc $1 | grep "q2 -> q2" | grep "guard P_0.CS";
    test `ltl_proc $1 | grep -c -- "->"` -eq 3
}

check_peterson2() {
    ltl_proc $1 | grep "q1 -> q2" | grep "guard not P_0.CS";
    ltl_proc $1 | grep "q1 -> q1" | grep "{}";
    ltl_proc $1 | grep "q2 -> q2" | grep "guard not P_0.CS";
    test `ltl_proc $1 | grep -c -- "->"` -eq 3
}

check_peterson3() {
    ltl_proc $1 | grep "q1 -> q2" | grep "guard not (in_critical==1)";
    ltl_proc $1 | grep "q1 -> q1" | grep "{}";
    ltl_proc $1 | grep "q2 -> q2" | grep "guard not (in_critical==1)";
    test `ltl_proc $1 | grep -c -- "->"` -eq 3
}


rm -f peterson.combined.dve
rm -f peterson.prop*.dve

divine combine data/peterson.dve -f data/peterson.ltl

for i in 1 2 3; do
    test -f peterson.prop$i.dve
    grep "process LTL_property" peterson.prop$i.dve
    grep "system async property LTL_property" peterson.prop$i.dve
    check_peterson$i peterson.prop$i.dve
done

divine combine data/peterson.mdve -o N=2 > peterson.combined.dve
not grep "process LTL_property" peterson.combined.dve
not grep "system async property LTL_property" peterson.combined.dve
grep "process P_0" peterson.combined.dve
grep "process P_1" peterson.combined.dve
not grep "process P_2" peterson.combined.dve

divine combine data/peterson.mdve N=2 -f data/peterson.ltl
for i in 1 2 3; do
    test -f peterson.prop$i.dve
    grep "process LTL_property" peterson.prop$i.dve
    grep "system async property LTL_property" peterson.prop$i.dve
    grep "process P_0" peterson.prop$i.dve
    grep "process P_1" peterson.prop$i.dve
    not grep "process P_2" peterson.prop$i.dve
    check_peterson$i peterson.prop$i.dve
done

