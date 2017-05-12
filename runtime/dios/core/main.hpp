// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#ifndef __DIOS_MAIN_H__
#define __DIOS_MAIN_H__

#include <utility>
#include <dios.h>
#include <dios/kernel.hpp>
#include <dios/core/stdlibwrap.hpp>

int main(...);

namespace __dios {

/*
 * Find all sys opt string (in form sys.#) and return key-value pairs
 */
bool getSysOpts( const _VM_Env *e, SysOpts& res );

String extractOpt( const String& key, SysOpts& opts );
bool extractOpt( const String& key, const String& value, SysOpts& opts );

/*
 * Construct null-terminated string from env->value
 */
char *env_to_string( const _VM_Env *env ) noexcept;

/*
 * Find env key by name, nullptr if key does not exist
 */
const _VM_Env *get_env_key( const char* key, const _VM_Env *e ) noexcept;

/*
 * Construct argv/envp-like arguments from env keys beginning with prefix.
 * If prepend_name, binary name is prepended to the arguments.
 * Return (argc, argv)
 */
std::pair<int, char**> construct_main_arg( const char* prefix, const _VM_Env *env,
    bool prepend_name = false ) noexcept;

/*
 * Trace arguments constructed by construct_main_arg
 */
void trace_main_arg( int indent, String name, std::pair< int, char** > arg );

/*
 * Free argv/envp-like arguments created by construct_main_arg
 */
void free_main_arg( char** argv ) noexcept;

void runCtors();
void runDtors();

} // namespace __dios

/*
 * DiOS main function, global constructors and destructor are called, return
 * value is checked. Variant defines number of arguments passed to main.
 *
 * note: _start must not be noexcept, otherwise clang generater invokes for
 * every function called from it and calls terminate in case of exception. This
 * then messes with standard behaviour of uncaught exceptions which should not
 * unwind stack.
 */
extern "C" void _start( int variant, int argc, char **argv, char **envp );


#endif // __DIOS_MAIN_H__
