//===-------------------------- random.cpp --------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "random"
#include "system_error"

#ifdef __sun__
#define rename solaris_headers_are_broken
#endif
#ifndef __divine__
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#endif

_LIBCPP_BEGIN_NAMESPACE_STD


#ifndef __divine__
random_device::random_device(const string& __token)
    : __f_(open(__token.c_str(), O_RDONLY))
{
    if (__f_ <= 0)
        __throw_system_error(errno, ("random_device failed to open " + __token).c_str());
}

random_device::~random_device()
{
    close(__f_);
}

unsigned
random_device::operator()()
{
    unsigned r;
    read(__f_, &r, sizeof(r));
    return r;
}

double
random_device::entropy() const _NOEXCEPT
{
    return 0;
}
#else

// FIXME: this is very very bad

random_device::random_device( const string & ) { }
random_device::~random_device() { }
unsigned random_device::operator()() {
    return random_device::min() + __divine_choice( random_device::max() - random_device::min() );
}

double random_device::entropy() const _NOEXCEPT {
    return 0;
}

#endif

_LIBCPP_END_NAMESPACE_STD
