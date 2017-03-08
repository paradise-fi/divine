#include <sys/syscall.h>
#include <sys/stat.h>

#define SYSCALL0(name, sched, ret) ret name( ) { \
	ret retval;\
	 __dios_syscall( SYS_ ## name  &retval,);\
    return retval; }
#define SYSCALL1(name, sched, ret, arg1, name1) ret name( arg1 name1) { \
	ret retval;\
	 __dios_syscall( SYS_ ## name, &retval, name1);\
    return retval; }
#define SYSCALL2(name, sched, ret, arg1, name1, arg2, name2) ret name( arg1 name1, arg2 name2) { \
	ret retval;\
	 __dios_syscall( SYS_ ## name, &retval, name1, name2 );\
    return retval; }
#define SYSCALL3(name, sched, ret, arg1, name1, arg2, name2, arg3, name3) ret name( arg1 name1, arg2 name2,  arg3 name3) { \
	ret retval;\
	 __dios_syscall( SYS_ ## name, &retval, name1, name2, name3 );\
    return retval; }
#define SYSCALL4(name, sched, ret, arg1, name1, arg2, name2,  arg3, name3,  arg4, name4) \
 ret name( arg1 name1, arg2 name2, arg3 name3,  arg4 name4) { \
	ret retval;\
	 __dios_syscall( SYS_ ## name, &retval, name1, name2, name3, name4 );\
    return retval; }
#define SYSCALL5(name, sched, ret, arg1, name1, arg2, name2,  arg3, name3,  arg4, name4, arg5, name5) \
    ret name( arg1 name1, arg2 name2, arg3 name3,  arg4 name4, arg5 name5) { \
	ret retval;\
	 __dios_syscall( SYS_ ## name, &retval, name1, name2, name3, name4, name5 );\
    return retval; }
#define SYSCALLSEP(...)
    #include <sys/syscall.def>
