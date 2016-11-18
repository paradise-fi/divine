. lib/testcase

touch a.c b.c
divine cc --dont-link a.c b.c
test -f a.c
test -f b.c
