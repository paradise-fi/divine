# TAGS: ext
. lib/testcase

unxz --version || skip
unxz < $SRC_ROOT/test/dioscc/gzip-1.8.tar.xz | tar x
cd gzip-1.8
./configure CC=dioscc CFLAGS=-O0
make
echo hello world | ./gzip - > hello.gz
divine check --stdin hello.gz ./gzip -f -d -
grep 'error found: no' gzip.report
