#include <ieeefp.h>
#include <fenv.h>
#include <sys/fault.h>

#define NOT_IMPLEMENTED { __dios_fault( _VM_F_NotImplemented, "floating point stubs" ); return 0; }

fp_except fpgetmask( void ) NOT_IMPLEMENTED;
fp_rnd fpgetround( void ) NOT_IMPLEMENTED;
fp_except fpgetsticky( void ) NOT_IMPLEMENTED;
fp_except fpsetmask( fp_except mask ) NOT_IMPLEMENTED;
fp_rnd fpsetround( fp_rnd rnd_dir ) NOT_IMPLEMENTED;
fp_except fpsetsticky( fp_except sticky ) NOT_IMPLEMENTED;

int fegetenv(fenv_t *envp) NOT_IMPLEMENTED;
int feholdexcept(fenv_t *envp) NOT_IMPLEMENTED;
int fesetenv(const fenv_t *envp) NOT_IMPLEMENTED;
int feupdateenv(const fenv_t *envp) NOT_IMPLEMENTED;
