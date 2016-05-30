#!/bin/sh

set -e

flatten() {
    sed -e "s|\\.|_|g" -e "s|/|_|g" -e "s|-|_|g" -e "s|+|_|g" -e "s|~|_|g"
}

NS=$1; shift
if ! [ -z $NS ]; then
    NS="src_${NS}"
    NSBEG="namespace ${NS} {"
    NSEND="}"
    NSPRE="${NS}::"
fi

if test "$1" = "-l"; then
    var="$2_list"
    out="$var.cpp"
    rm -f $out
    shift 2
    echo "namespace divine {" >> $out
    echo $NSBEG >> $out

    for n in "$@"; do
        echo "extern const char *$(echo $n | flatten)_str;" >> $out
    done

    echo $NSEND >> $out

    echo "struct stringtable { const char *n, *c; };" >> $out
    echo "stringtable ${var}[] = { " >> $out
    for n in "$@"; do
        var=$(echo "$n" | flatten)
        echo "{ \"$n\", ${NSPRE}${var}_str }," >> $out
    done
    echo "{ 0, 0 }" >> $out
    echo "};" >> $out
    echo "}" >> $out
else
    flat=$(echo $2 | flatten)
    (cat "$1/$2"; echo) | sed \
         -e 's,\\,\\\\,g' \
         -e 's,",\\",g' \
         -e 's,$,\\n",' \
         -e 's,^,",' \
         -e '1i \
namespace divine {\
'"${NSBEG}"'\
const char *'"${flat}"'_str =\
'\
         -e '$a\
;\
}\
'"${NSEND}" \
        > "${flat}_str.cpp"
fi
