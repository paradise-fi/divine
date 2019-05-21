# TAGS: divcc
. lib/testcase

cat > strerr_gnu.c <<EOF
#include <string.h>
#include <assert.h>

int main()
{
    char buf[25];
    char * ret = strerror_r( 3, buf, 25 );
    assert( strcmp( ret, "No such process" ) == 0 );
}
EOF

divcc -o gnu -D_GNU_SOURCE strerr_gnu.c

if [ -s a.out ] || ! [ -s gnu ] || ! [ -x gnu ];
    then false;
fi

./gnu

divine check gnu
divine exec --virtual gnu | grep "No such process"


cat > strerr_nognu.c <<EOF
#include <string.h>
#include <assert.h>

int main()
{
    char buf[25];
    int ret = strerror_r( 3, buf, 25 );
    assert( ret == 0 );
    assert( strcmp( buf, "No such process" ) == 0 );
}
EOF

divcc -o nognu -D_XOPEN_SOURCE strerr_nognu.c

if [ -s a.out ] || ! [ -s nognu ] || ! [ -x nognu ];
    then false;
fi

./nognu

divine check nognu
divine exec --virtual nognu | grep "No such process"
