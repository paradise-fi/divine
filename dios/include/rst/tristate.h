#include <rst/common.h>

#ifndef __ABSTRACT_TRISTATE_H_
#define __ABSTRACT_TRISTATE_H_

namespace abstract {

struct Tristate {
	enum Domain { False = 0, True = 1, Unknown = 2 };

    Tristate( Domain val ) : value( val ) { }

	Domain value;
};

extern "C" {
    bool __abstract_tristate_lower( Tristate * tristate ) _ROOT _NOTHROW;
}

} // namespace abstract
#endif //__ABSTRACT_TRISTATE_H_
