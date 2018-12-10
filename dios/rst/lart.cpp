#include <rst/lart.h>
#include <sys/task.h>
#include <rst/common.h>
#include <sys/divm.h>

#include <string.h>

extern "C"
{
    uintptr_t __lart_unstash()
    {
        return __dios_this_task()->__rst_stash;
    }

    void __lart_stash( uintptr_t val )
    {
        __dios_this_task()->__rst_stash = val;
    }

    #define PLACEHOLDER( out, op, ... ) out __lart_string_op_ ## op( __VA_ARGS__ ) { \
        _UNREACHABLE_F("LART PLACEHOLDER");                                         \
    }

    /* String manipulation */
    PLACEHOLDER( char *, strcpy , char * /* dest */, const char * /* src */ );
    PLACEHOLDER( char *, strncpy, char * /* dest */, const char * /* src */, size_t  /* count */ );
    PLACEHOLDER( char *, strcat , char * /* dest */, const char * /* src */ );
    PLACEHOLDER( char *, strncat, char * /* dest */, const char * /* src */, size_t  /* count */ );
    PLACEHOLDER( size_t, strxfrm, char * /* dest */, const char * /* src */, size_t  /* count */ );

    /* String examination */
    PLACEHOLDER( size_t, strlen , const char * /* str */ );
    PLACEHOLDER( int   , strcmp , const char * /* lhs */ , const char * /* rhs */ );
    PLACEHOLDER( int   , strncmp, const char * /* lhs */ , const char * /* rhs */, size_t  /* count */ );
    PLACEHOLDER( int   , strcoll, const char * /* lhs */ , const char * /* rhs */ );
    PLACEHOLDER( char *, strchr , const char * /* str */ , int /* ch */ );
    PLACEHOLDER( char *, strrchr, const char * /* str */ , int /* ch */ );
    PLACEHOLDER( size_t, strspn , const char * /* dest */, const char * /* src */ );
    PLACEHOLDER( size_t, strcspn, const char * /* dest */, const char * /* src */ );
    PLACEHOLDER( char *, strpbrk, const char * /* dest */, const char * /* breakset */ );
    PLACEHOLDER( char *, strstr , const char * /* str */ , const char * /*substr */ );
    PLACEHOLDER( char *, strtok , char * /* str */       , const char * /* delim */ );
}
