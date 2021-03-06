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

// <iterator>

// back_insert_iterator

// back_insert_iterator<Cont>& operator*();

#include <iterator>
#include <vector>
#include <cassert>
#include "nasty_containers.hpp"

#include "test_macros.h"

template <class C>
void
test(C c)
{
    std::back_insert_iterator<C> i(c);
    std::back_insert_iterator<C>& r = *i;
    assert(&r == &i);
}

int main(int, char**)
{
    test(std::vector<int>());
    test(nasty_vector<int>());

  return 0;
}
