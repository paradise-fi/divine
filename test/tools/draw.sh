. lib

draw() {
    divine draw -r cat -o out.dot $1
    diff -u out.dot $1.dot

    divine draw -l -r cat -o out.dot $1
    grep '\->' out.dot | not grep -v '\[.*label.*\]'
}

dve_small draw
