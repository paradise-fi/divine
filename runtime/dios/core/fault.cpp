// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>
//                 2016 Vladimir Still <xstill@fi.muni.cz>

#include <algorithm>
#include <cctype>

#include <dios/core/fault.hpp>
#include <dios/core/trace.hpp>
#include <dios/core/syscall.hpp>

uint8_t const *_DiOS_fault_cfg;

namespace __dios { }
