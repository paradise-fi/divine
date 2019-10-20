#include <stdio.h>
/* EXPECT: --trace non-zero --symbol _Exit --result error */

int main()
{
    printf( "foobar: %d\n", 0 );
    return 1;
}
