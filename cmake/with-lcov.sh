#!/bin/sh

LCOV() {
       "${LCOV_BIN}" -d "${LCOV_IN}" --quiet "$@"
}

set -ex

mkdir -p "${LCOV_OUT}"

if ! test -e "${LCOV_OUT}/failed" && ! test -e "${LCOV_OUT}/base.info"; then
    LCOV --zerocounters
    LCOV --capture --initial -o "${LCOV_OUT}/base.info" || touch "${LCOV_OUT}/failed"
fi

eval "$@"

test -e "${LCOV_OUT}/failed" && exit 0

for i in `seq 1 32`; do
    test -f "${LCOV_OUT}/collect-$i.info" && continue
    LCOV --gcov-tool="${GCOV_BIN}" -b "${LCOV_IN}" -c -o "${LCOV_OUT}/collect-$i.info" 2>/dev/null
    break
done
LCOV --zerocounters
