#pragma once

#include <rst/split.h>

typedef abstract::mstring::Split __mstring;

#define DOMAIN_NAME mstring
#define DOMAIN_KIND content
#define DOMAIN_TYPE __mstring *
    #include <rst/string-domain.def>
#undef DOMAIN_NAME
#undef DOMAIN_TYPE
#undef DOMAIN_KIND

extern "C" {
    __mstring * __mstring_undef_value();
}
