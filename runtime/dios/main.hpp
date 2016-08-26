// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#ifndef __DIOS_MAIN_H__
#define __DIOS_MAIN_H__

#include <utility>
#include <dios.h>

namespace __dios {

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
 * Free argv/envp-like arguments created by construct_main_arg
 */
void free_main_arg( char** argv ) noexcept;

} // namespace __dios

/*
 * DiOS main function, global constructors and destructor are called, return
 * value is checked. Variant defines number of arguments passes to main
 */
extern "C" void __dios_main( int variant, int argc, char **argv, char **envp ) noexcept;


#endif // __DIOS_MAIN_H__
