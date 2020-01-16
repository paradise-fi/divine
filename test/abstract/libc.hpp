#pragma once

#include <cstring>

namespace libc
{

    static size_t strlen( const char * str )
    {
        const char * a = str;
        for (; *str; str++) {}
        return str - a;
    }

    static char * strcpy( char * dst, const char * src )
    {
        for (; (*dst=*src); src++, dst++);
        return dst;
    }

    static char * strcat( char * dst, const char * src )
    {
        libc::strcpy( dst + libc::strlen( dst ) , src );
        return dst;
    }

    static int strcmp( const char * lhs, const char * rhs )
    {
        for (; *lhs==*rhs && *lhs; lhs++, rhs++);
        return *(unsigned char *)lhs - *(unsigned char *)rhs;
    }

    static char *strchrnul(const char *s, int c)
    {
        c = (unsigned char)c;
        if (!c) return (char *)s + libc::strlen(s);

        for (; *s && *(unsigned char *)s != c; s++);
        return (char *)s;
    }

    static char *strchr(const char *s, int c)
    {
        char *r = libc::strchrnul(s, c);
        return *(unsigned char *)r == (unsigned char)c ? r : 0;
    }

} // namespace libc
