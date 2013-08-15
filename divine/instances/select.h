// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>

#include <divine/algorithm/common.h>
#include <memory>

#ifndef DIVINE_INSTANTIATE_SELECT
#define DIVINE_INSTANTIATE_SELECT

namespace divine {

std::unique_ptr< ::divine::algorithm::Algorithm > select( Meta &meta );

}

#endif // DIVINE_INSTANTIATE_SELECT
