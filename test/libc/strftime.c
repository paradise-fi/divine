#include <time.h>
#include <assert.h>
#include <string.h>

int main()
{
    struct tm timeval;
    timeval.tm_mon = 5;
    timeval.tm_mday = 20;
    timeval.tm_year = 2018;
    auto char tbuf[100];

    strftime(tbuf, sizeof tbuf, "%m/%d/%y", &timeval);
    assert( strcmp(tbuf, "06/20/18") == 0 );
}
