#pragma once

#include <cstddef>
#include <cstdint>

#include <rst/domains.h>
#include <rst/common.h>
#include <rst/lart.h>


namespace abstract::star {
    enum class Unit : uint8_t { };
} // namespace abstract::star

typedef abstract::star::Unit __unit;

#define DOMAIN_NAME star
#define DOMAIN_KIND scalar
#define DOMAIN_TYPE __unit
    #include <rst/integer-domain.def>
#undef DOMAIN_NAME
#undef DOMAIN_TYPE
#undef DOMAIN_KIND
