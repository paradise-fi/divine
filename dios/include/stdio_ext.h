#ifndef _STDIO_EXT_H
#define _STDIO_EXT_H

#include <stdio.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

enum
{
    FSETLOCKING_QUERY = 0,
    FSETLOCKING_INTERNAL,
    FSETLOCKING_BYCALLER
};

size_t __fbufsize (FILE *__fp) __nothrow;
int __freading (FILE *__fp) __nothrow;
int __fwriting (FILE *__fp) __nothrow;
int __freadable (FILE *__fp) __nothrow;
int __fwritable (FILE *__fp) __nothrow;
int __flbf (FILE *__fp) __nothrow;
void __fpurge (FILE *__fp) __nothrow;
size_t __fpending (FILE *__fp) __nothrow;
int __fsetlocking (FILE *__fp, int __type) __nothrow;

size_t __freadahead(FILE *) __nothrow;
const char *__freadptr(FILE *, size_t *) __nothrow;
void __freadptrinc(FILE *, size_t) __nothrow;
void __fseterr(FILE *) __nothrow;

__END_DECLS

#endif
