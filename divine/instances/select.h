// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>

#include <memory>
#include <divine/algorithm/common.h>

#ifndef DIVINE_INSTANTIATE_SELECT
#define DIVINE_INSTANTIATE_SELECT

namespace divine {

using AlgorithmPtr = std::unique_ptr< algorithm::Algorithm >;
AlgorithmPtr select( Meta &meta );

}

#endif // DIVINE_INSTANTIATE_SELECT
