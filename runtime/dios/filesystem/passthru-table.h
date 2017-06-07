#ifndef __DIOS_PASSTHRU_TABLE__
#define __DIOS_PASSTHRU_TABLE__
/*
* Generating table for mapping syscall name to its number inside
* works only for Linux x86 and x86_64 architecture
* definition in syscal.hpp
*/
namespace __dios {

#define SYSCALL(num,name) _VM_SC_ ## name = num,
	enum _VM_SC {
	    #include <dios/core/systable.def>
	};

#undef SYSCALL

} //dios

#endif // __DIOS_PASSTHRU_TABLE__