set -vex
not () { "$@" && exit 1 || return 0; }

rm -f peterson.combined.dve
rm -f peterson.prop*.dve

divine combine peterson.dve -f peterson.ltl

for i in 1 2 3; do
    test -f peterson.prop$i.dve
    grep "process LTL_property" peterson.prop$i.dve
    grep "system async property LTL_property" peterson.prop$i.dve
done

divine combine peterson.mdve -o N=2 > peterson.combined.dve
not grep "process LTL_property" peterson.combined.dve
not grep "system async property LTL_property" peterson.combined.dve
grep "process P_0" peterson.combined.dve
grep "process P_1" peterson.combined.dve
not grep "process P_2" peterson.combined.dve

divine combine peterson.mdve N=2 -f peterson.ltl
for i in 1 2 3; do
    test -f peterson.prop$i.dve
    grep "process LTL_property" peterson.prop$i.dve
    grep "system async property LTL_property" peterson.prop$i.dve
    grep "process P_0" peterson.prop$i.dve
    grep "process P_1" peterson.prop$i.dve
    not grep "process P_2" peterson.prop$i.dve
done

