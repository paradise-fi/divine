/** -*- C++ -*-
libLYOD -- An implementation of the ICE protocol, as specified in:

    "Inter-Client Exchance (ICE) Protocol, Version 1.1"

libLYOD copyright (c) 2004 Maksim Orlovich <mo85@cornell.edu>

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#ifndef WIBBLE_SHARED_H
#define WIBBLE_SHARED_H

#include <cassert>

namespace wibble {

/**
 Base for reference-counted objects
*/
class SharedBase
{
protected:
    SharedBase() : _refCount(0)
    {}

public:
    typedef void SharedBaseTag;
    void _ref()
    {
        assert(_refCount >= 0);
        ++_refCount;
    }

    void _unref()
    {
        assert(_refCount >= 0);
        --_refCount;
        if (_refCount == 0)
            delete this;
    }

    int _get_refCount() { return _refCount; }

    virtual ~SharedBase() {}

private:
    int _refCount;
};

template< typename T >
typename T::SharedBaseTag reference( T *x ) { x->_ref(); }

// template< typename T >
// void reference( T *x ) {}

template< typename T >
typename T::SharedBaseTag unreference( T *x ) { x->_unref(); }

// template< typename T >
// void unreference( T *x ) {}

/**
 Reference-counting pointer wrapper. Note that the type must
 inherit off SharedBase, or compatible.
*/
template<typename T>
class SharedPtr
{
private:
    T* ptr;
public:
    SharedPtr():ptr(0)
    {}

    SharedPtr(T* _ptr):ptr(_ptr)
    {
        if (ptr)
            reference( ptr );
    }

    SharedPtr(const SharedPtr<T>& other)
    {
        ptr = other.ptr;
        if (ptr)
            reference( ptr );
    }

    ~SharedPtr()
    {
        if (ptr)
            unreference( ptr );
    }

    bool operator <  (const SharedPtr<T>& other) const
    {
        return ptr < other.ptr;
    }

    bool operator == (const SharedPtr<T>& other) const
    {
        return ptr == other.ptr;
    }

    operator bool() const
    {
        return ptr;
    }

    T* data()
    {
        return ptr;
    }

    T* operator->()
    {
        return ptr;
    }

    const T* operator->() const
    {
        return ptr;
    }

    SharedPtr<T>& operator=(const SharedPtr<T>& other)
    {
        //Must do it in this order, in case other is this...
        if (other.ptr)
            reference( other.ptr );

        if (ptr)
            unreference( ptr );

        ptr = other.ptr;
        return *this;
    }
};

template< typename Base >
struct Shared : Base, SharedBase {};

template< typename T >
Shared< T > &shared() {
    return *(new Shared< T >());
}

}

#endif
// kate: indent-width 4; replace-tabs on; tab-width 4; space-indent on;
