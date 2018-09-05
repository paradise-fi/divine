#pragma once
namespace abstract {

struct Zero {
    enum Domain { ZeroValue, NonzeroValue, Unknown };

    Zero( Domain val = Unknown ) : value( val ) { }

    Domain value;
};

} // namespace abstract

typedef abstract::Zero __zero;

#define DOMAIN_NAME zero
#define DOMAIN_KIND scalar
#define DOMAIN_TYPE __zero
    #include <rst/domain.def>
#undef DOMAIN_NAME
#undef DOMAIN_TYPE
#undef DOMAIN_KIND
