#include <_PDCLIB/cdefs.h>

#ifndef _SYS_START_H
#define _SYS_START_H

_PDCLIB_EXTERN_C

/*
 * DiOS main function, global constructors and destructor are called, return
 * value is checked. Variant defines number of arguments passed to main.
 *
 * note: _start must not be noexcept, otherwise clang generater invokes for
 * every function called from it and calls terminate in case of exception. This
 * then messes with standard behaviour of uncaught exceptions which should not
 * unwind stack.
 */
void __dios_start( int variant, int argc, char **argv, char **envp );
void __dios_start_synchronous( int variant, int argc, char **argv, char **envp );

void __dios_run_ctors();
void __dios_run_dtors();

void __pthread_start( void * );
void __pthread_entry( void * );

#ifdef __cplusplus
int main(...);
#else
int main();
#endif

_PDCLIB_EXTERN_END

#endif
