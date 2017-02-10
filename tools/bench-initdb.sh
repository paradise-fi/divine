#!/bin/sh

dir=$(dirname $0)

if test "$1" = "--postgres"; then
    cat $dir/bench-postgres.sql $dir/bench-common.sql | sed -e s,blob,bytea,
elif test "$1" = "--sqlite"; then
    cat $dir/bench-common.sql | sed -e s,serial,integer,
else
    echo "HINT: run me with --postgres or --sqlite" >&2
    echo "like this: bench-initdb.sh --sqlite | sqlite3 ./test.sqlite" >&2
    echo "or: bench-initdb.sh --postgres | psql -h dbhost" >&2
fi
