// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>

#include <memory>
#include <divine/algorithm/common.h>

#ifndef DIVINE_INSTANTIATE_SELECT
#define DIVINE_INSTANTIATE_SELECT

namespace divine {

namespace instantiate {

struct SupportedBy;
struct Key;
bool _evalSuppBy( const SupportedBy &suppBy, const std::vector< Key > &vec );
bool _valid( std::vector< Key > trace );

}

using AlgorithmPtr = std::unique_ptr< algorithm::Algorithm >;
AlgorithmPtr select( Meta &meta );

}

#endif // DIVINE_INSTANTIATE_SELECT
