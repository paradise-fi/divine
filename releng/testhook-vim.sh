#!/bin/sh
mkdir -p $SRCDIR/_failed_tests
rm -f $SRCDIR/_failed_tests/*
egrep -v 'passed|skipped' results/list | while read f g; do
    cp results/$(echo $f | sed -e s,/,_,g).txt $SRCDIR/_failed_tests/
done
egrep -qv 'passed|skipped' results/list && vim $SRCDIR/_failed_tests/
