# TAGS: min
. lib/testcase

SRC=$TESTS/lang-c/assert.c

divine verify --report-filename verify.out $SRC

sim --load-report verify.out <<EOF
+ ^# .*frame in __dios_fault
> backtrace
EOF
