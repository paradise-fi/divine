// -*- C++ -*-
#include <typeinfo>
#ifndef _APTFRONT_CAST
#define _APTFRONT_CAST

namespace aptFront {

template <typename T, typename X> T &downcast(X *v) {
    if (!v)
        throw std::bad_cast();
    T *x = dynamic_cast<T *>(v);
    return *x;
}

}

#endif
