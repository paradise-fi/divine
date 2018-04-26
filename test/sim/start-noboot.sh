# TAGS: min
. lib/testcase

sim $TESTS/lang-c/assert.c <<EOF
+ ^# executing __boot at
> setup --debug-everything
> start
- ^# executing
+ ^# executing main at
> start --no-boot
+ ^# executing __boot at
> stepa
+ ^# executing.*run_scheduler
EOF
