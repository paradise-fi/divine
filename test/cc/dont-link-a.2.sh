. lib/testcase

touch a.c b.c
divine cc -c a.c b.c
test -f a.c
test -f b.c
