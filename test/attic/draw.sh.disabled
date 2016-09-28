. lib
. flavour vanilla 'part+*'
FLAVOUR=`echo $FLAVOUR | sed 's/--no-shared//'`

normal() {
    diff -u out.dot $1.dot
}

labels() {
    (grep -- '->' out.dot || true) | not grep -v '\[.*label.*\]'
}

test "$WIN32" = "1" && skip

extracheck=normal
dve_small draw -r cat -o out.dot $FLAVOUR

extracheck=labels
dve_small draw -l -r cat -o out.dot $FLAVOUR
