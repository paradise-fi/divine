#pragma once
#include <divine/ui/common.hpp>

namespace divine {
namespace ui {

struct Curses : Interface
{
    virtual int main() override
    {
        return 0;
    }
};

}
}
