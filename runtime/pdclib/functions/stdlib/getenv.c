// -*- C -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#include <unistd.h>
#include <stdlib.h>

/**
 * Return first character from s after prefix pref. If pref is not a prefix of s
 * return NULL
 */
static char *is_prefix_of( const char *pref, char *s ) {
    for ( ; *pref && *s; ++pref, ++s ) {
        if ( *pref != *s )
            return NULL;
    }
    if( ! *pref )
        return s;
    return NULL;
}

char *getenv( const char* env_var ) {
    if ( !env_var || !environ )
        return NULL;
    char **env = environ;
    for ( ; *env; ++env ) {
        char *ret = is_prefix_of( env_var, *env );
        if ( ret && *ret == '=' )
            return ret + 1;
    }
    return NULL;
}
