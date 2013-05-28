#!/bin/sh

LCOV() {
       "${LCOV_BIN}" -d "${LCOV_IN}" "$@"
}

set -ex

mkdir -p "${LCOV_OUT}"

test -e "${LCOV_OUT}/base.info" || {
    LCOV --zerocounters
    LCOV --capture --initial -o "${LCOV_OUT}/base.info"
}

eval "$@"

for i in `seq 1 32`; do
    test -f "${LCOV_OUT}/collect-$i.info" && continue
    LCOV --gcov-tool="${GCOV_BIN}" --quiet -b "${LCOV_IN}" -c -o "${LCOV_OUT}/collect-$i.info"
    break
done
LCOV --zerocounters
