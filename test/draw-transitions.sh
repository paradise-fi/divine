draw() {
    divine draw -l -r cat -o "$@"
}

test_labels() {
    grep '\->' $1 | grep -v '\[.*label.*\]'
    if [ $? -eq 0 ]; then exit 1; fi
}

tmpfile() {
    TEMPPREFIX=$(basename "$0")
    mktemp /tmp/${TEMPPREFIX}.XXXXXX
}

OUT=$(tmpfile)

trap 'rm -f $OUT' EXIT

for t in 1 2 3 4 5 6; do
    draw $OUT test$t.dve
    test_labels $OUT
done
