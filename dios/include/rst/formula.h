#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <sys/lart.h>

#ifndef __RST_FORMULA_H__
#define __RST_FORMULA_H__

#if __cplusplus >= 201103L
#include <string>

namespace lart::sym {

std::string toString( Op x );
std::string toString( Type );
std::string toString( const Formula *f );
inline std::string toString( const Formula &f ) { return toString( &f ); }

} // namespace sym

#endif

#endif // __BVEC_H__
