// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>
//                 2016 Vladimir Still <xstill@fi.muni.cz>

#ifndef __DIOS_FAULT_H__
#define __DIOS_FAULT_H__

#include <dios.h>

namespace __dios {

void fault( enum _VM_Fault what, _VM_Frame *cont_frame, void (*cont_pc)() ) noexcept __attribute__((__noreturn__));

} // namespace __dios


#endif // __DIOS_FAULT_H__
