// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>

#include <type_traits>

#include <wibble/sfinae.h>

#include <divine/instances/definitions.h>

#ifndef DIVINE_INSTANCES_SELECT_IMPL
#define DIVINE_INSTANCES_SELECT_IMPL

namespace divine {
namespace instantiate {

template< typename T >
struct IsMissing : std::is_same< T, _Missing > { };

template< typename List >
struct AllMissing {
    static constexpr bool value = IsMissing< typename List::Head >::value
        && AllMissing< typename List::Tail >::value;
};

template<>
struct AllMissing< TypeList<> > {
    static constexpr bool value = true;
};

template< typename List >
using NotMissing = Filter< IsMissing, List >;

template< typename List >
struct Default : public Last< typename NotMissing< List >::T > { };


/*****************************************************************************/

template< typename Selector, typename Head, typename Tail, typename Default,
    typename ToSelect, typename Selected >
auto runIfValid( Selector &sel, Head, Tail, Default, ToSelect, Selected,
        wibble::Preferred, wibble::Preferred )
    -> typename std::enable_if< std::is_base_of< InstantiationError, Head >::value,
        typename Selector::ReturnType >::type
{
    return runOne( sel, Tail(), Default(), ToSelect(), Selected() );
}

template< typename Selector, typename Head, typename Tail, typename Default,
    typename ToSelect, typename Selected >
auto runIfValid( Selector &sel, Head, Tail, Default, ToSelect, Selected,
        wibble::Preferred, wibble::NotPreferred )
    -> typename std::enable_if<
        ! std::is_base_of< InstantiationError, Head >::value
        && EvalBoolExpr< ContainsP< Selected >::template Elem,
            typename Head::SupportedBy >::value,
        typename Selector::ReturnType >::type
{
    return sel.ifSelect( Head(), [ &sel ]() {
            return runSelector( sel, ToSelect(),
                typename Append< Head, Selected >::T() );
        }, [ &sel ]() {
            return runOne( sel, Tail(), Default(), ToSelect(), Selected() );
        } );
}

template< typename Selector, typename Head, typename Tail, typename Default,
    typename ToSelect, typename Selected >
auto runIfValid( Selector &sel, Head, Tail, Default, ToSelect, Selected,
        wibble::NotPreferred, wibble::NotPreferred )
    -> typename Selector::ReturnType
{
    return runOne( sel, Tail(), Default(), ToSelect(), Selected() );
}

template< typename Selector, typename Default, typename ToSelect, typename Selected >
auto selectDefault( Selector &sel, Default, ToSelect, Selected )
    -> typename std::enable_if< std::is_base_of< InstantiationError,
        Default >::value, typename Selector::ReturnType >::type
{
    return sel.instantiationError( Default() );
}

template< typename Selector, typename Default, typename ToSelect, typename Selected >
auto selectDefault( Selector &sel, Default, ToSelect, Selected )
    -> typename std::enable_if< !std::is_base_of< InstantiationError,
        Default >::value, typename Selector::ReturnType >::type
{
    return runSelector( sel, ToSelect(),
            typename Append< Default, Selected >::T() );
}

template< typename Selector, typename Current, typename Default,
    typename ToSelect, typename Selected >
auto runOne( Selector &sel, Current, Default, ToSelect, Selected )
    -> typename std::enable_if< ( Current::length > 0 ),
        typename Selector::ReturnType >::type
{
    using Head = typename Current::Head;
    using Tail = typename Current::Tail;

    return runIfValid( sel, Head(), Tail(), Default(), ToSelect(), Selected(),
            wibble::Preferred(), wibble::Preferred() );
}


template< typename Selector, typename Current, typename Default,
    typename ToSelect, typename Selected >
auto runOne( Selector &sel, Current, Default, ToSelect, Selected )
    -> typename std::enable_if< Current::length == 0, typename Selector::ReturnType >::type
{
    return selectDefault( sel, Default(), ToSelect(), Selected() );
}


template< typename Selector, typename ToSelect, typename Selected >
auto runSelector( Selector &sel, ToSelect, Selected )
    -> typename Selector::ReturnType
{
    using Head = typename NotMissing< typename ToSelect::Head >::T;
    using Default = typename Default< Head >::T;
    using Tail = typename ToSelect::Tail;
    return runOne( sel, Head(), Default(), Tail(), Selected() );
}

template< typename Selector, typename Selected >
auto runSelector( Selector &sel, TypeList<>, Selected )
    -> typename Selector::ReturnType
{
    return sel.template create< typename Reverse< Selected >::T >();
}

}
}

#endif // DIVINE_INSTANCES_SELECT_IMPL
