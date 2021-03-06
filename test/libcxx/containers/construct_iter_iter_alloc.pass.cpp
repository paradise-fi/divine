/* TAGS: c++ fin */
/* CC_OPTS: -std=c++2a */
/* VERIFY_OPTS: -o nofail:malloc */
//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// <vector>

// template <class InputIter> vector(InputIter first, InputIter last,
//                                   const allocator_type& a);

#include <vector>
#include <cassert>

#include "test_macros.h"
#include "min_allocator.h"

void test_ctor_under_alloc() {
  int arr1[] = {42};
  int arr2[] = {1, 101, 42};
  {
    typedef std::vector<int, cpp03_allocator<int> > C;
    typedef C::allocator_type Alloc;
    Alloc a;
    {
      Alloc::construct_called = false;
      C v(arr1, arr1 + 1, a);
      assert(Alloc::construct_called);
    }
    {
      Alloc::construct_called = false;
      C v(arr2, arr2 + 3, a);
      assert(Alloc::construct_called);
    }
  }
  {
    typedef std::vector<int, cpp03_overload_allocator<int> > C;
    typedef C::allocator_type Alloc;
    Alloc a;
    {
      Alloc::construct_called = false;
      C v(arr1, arr1 + 1, a);
      assert(Alloc::construct_called);
    }
    {
      Alloc::construct_called = false;
      C v(arr2, arr2 + 3, a);
      assert(Alloc::construct_called);
    }
  }
}

int main(int, char**) {
  test_ctor_under_alloc();

  return 0;
}
