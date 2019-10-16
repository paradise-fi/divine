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
// UNSUPPORTED: c++98, c++03
// UNSUPPORTED: windows

// MODULES_DEFINES: _LIBCPP_DEBUG=1

// Can't test the system lib because this test enables debug mode
// UNSUPPORTED: with_system_cxx_lib

// test array<T, 0>::operator[] raises a debug error.

#define _LIBCPP_DEBUG 1
#include <array>
#include "test_macros.h"
#include "debug_mode_helper.h"

// V: fstone CC_OPT: -DFST_ONE
// V: fsttwo CC_OPT: -DSFT_TWO
// V: fstthree CC_OPT: -DFST_THREE
// V: fstfour CC_OPT: -DFST_FOUR


// V: sndone CC_OPT: -DSND_ONE
// V: sndtwo CC_OPT: -DSFT_TWO
// V: sndthree CC_OPT: -DSND_THREE
// V: sndfour CC_OPT: -DSND_FOUR

int main(int, char**)
{
  {
    typedef std::array<int, 0> C;
    C c = {};
    C const& cc = c;
#ifdef FST_ONE
    EXPECT_DEATH( c[0] ); /* ERR_fstone */
#endif

#ifdef FST_TWO
    EXPECT_DEATH( c[1] ); /* ERR_fsttwo */
#endif

#ifdef FST_THREE
    EXPECT_DEATH( cc[0] ); /* ERR_fstthree */
#endif

#ifdef FST_FOUR
    EXPECT_DEATH( cc[1] ); /* ERR_fstthree */
#endif
  }
  {
    typedef std::array<const int, 0> C;
    C c = {{}};
    C const& cc = c;

#ifdef SND_ONE
    EXPECT_DEATH( c[0] ); /* ERR_sndone */
#endif

#ifdef SND_TWO
    EXPECT_DEATH( c[1] ); /* ERR_sndtwo */
#endif

#ifdef SND_THREE
    EXPECT_DEATH( cc[0] ); /* ERR_sndthree */
#endif

#ifdef SND_FOUR
    EXPECT_DEATH( cc[1] ); /* ERR_sndthree */
#endif

  }

  return 0;
}
