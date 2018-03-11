#include <sys/divm.h>
#include <sys/env.h>

const struct _VM_Env __sys_env[];
const struct _VM_Env *__sys_env_get() { return __sys_env; }
