#pragma once

namespace divine {
namespace cc {

struct Options
{
    int num_threads;
    bool dont_link, libs_only;
    std::string precompiled;
    Options() : num_threads( 1 ), dont_link( false ), libs_only( false ) {}
};

}
}
