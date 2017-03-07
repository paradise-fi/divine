#include <stdio.h>
#include <stdarg.h>

int asprintf( char **result, const char * _PDCLIB_restrict format, ... )
{
    int rc;
    va_list ap;
    va_start( ap, format );
    rc = vasprintf( result, format, ap );
    va_end( ap );
    return rc;
}
