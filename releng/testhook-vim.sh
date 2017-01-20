#!/bin/sh
mkdir -p $SRCDIR/_failed_tests
rm -f $SRCDIR/_failed_tests/*
grep -v passed results/list | while read f g; do
    cp results/$(echo $f | sed -e s,/,_,g).txt $SRCDIR/_failed_tests/
done
grep -qv passed results/list && vim $SRCDIR/_failed_tests/
