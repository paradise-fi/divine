// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>

#ifndef WIBLLE_PARAM_H
#define WIBLLE_PARAM_H

namespace wibble {
namespace param {

#if __cplusplus >= 201103L

/* discard any number of paramentets, taken as const references */
template< typename... X >
void discard( const X&... ) { }

#endif

}
}

#endif // WIBLLE_PARAM_H
