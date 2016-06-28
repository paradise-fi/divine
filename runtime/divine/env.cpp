#include <divine.h>
#include <divine/__env.h>

extern "C" extern const _VM_Env __sys_env[];
const _VM_Env *__sys_env_get() { return __sys_env; }
