# TAGS: min
. lib/testcase

draw $TESTS/lang-c/assert.c | grep -i 'label.*=.*assert'
draw $TESTS/lang-c/assert.c | grep -i 'color=red' # error edges are red
