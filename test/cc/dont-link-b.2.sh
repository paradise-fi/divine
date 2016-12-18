. lib/testcase

touch a.c b.c
divine cc --dont-link a.c b.c
test -f a.bc
test -f b.bc
