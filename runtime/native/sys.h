// Taken from MUSL library, edited by Vladimir Still

#ifndef	__DIVINE_NATIVE_SYS_H__
#define	__DIVINE_NATIVE_SYS_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

void *__native_mmap_anon( size_t size );
_Noreturn void __native_exit( int ec );
long __native_write( int fd, const void *buf, size_t count );
void __native_putStr( const char *buf, size_t count );
void __native_putErrStr( const char *buf, size_t count );

#ifdef __cplusplus
}
#endif
#endif
