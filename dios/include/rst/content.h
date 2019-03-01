// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

namespace abstract::content {

    struct Memory
    {
        using Data = char *;
        Data data;
    };

} // namespace abstract::content

typedef abstract::content::Memory __content;

#define DOMAIN_NAME content
#define DOMAIN_KIND content
#define DOMAIN_TYPE __content *
    #include <rst/content-domain.def>
#undef DOMAIN_NAME
#undef DOMAIN_TYPE
#undef DOMAIN_KIND
