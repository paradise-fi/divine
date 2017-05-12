#ifndef _SYS_START_H
#define _SYS_START_H

#ifdef __cplusplus
#define EXTERN_C extern "C"
#endif

/*
 * DiOS main function, global constructors and destructor are called, return
 * value is checked. Variant defines number of arguments passed to main.
 *
 * note: _start must not be noexcept, otherwise clang generater invokes for
 * every function called from it and calls terminate in case of exception. This
 * then messes with standard behaviour of uncaught exceptions which should not
 * unwind stack.
 */
EXTERN_C void _start( int variant, int argc, char **argv, char **envp );

EXTERN_C void __dios_run_ctors();

EXTERN_C void __dios_run_dtors();

EXTERN_C int main(...);

#endif // _SYS_START_H