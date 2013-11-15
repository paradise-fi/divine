// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>

#include <type_traits>
#include <vector>

#include <wibble/sfinae.h>
#include <cctype>

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


// test what could have been selected (note that we work with copy of selector!)
template< typename Selector, typename Others >
void fillOthers( Selector sel, std::vector< Atom > &others, Others ) {
    sel.ifSelect( typename Others::Head(), [ sel, &others ]() -> typename Selector::ReturnType {
            others.emplace_back( typename Others::Head() );
            fillOthers( sel, others, typename Others::Tail() );
            return typename Selector::ReturnType();
        }, []() { return typename Selector::ReturnType(); } );
}

template< typename Selector >
void fillOthers( const Selector &, std::vector< Atom > &, TypeList<> ) { }

// find out what would be selected and warn about it if that was requested
template< typename Selector, typename SelOpt, typename Selected, typename Others >
auto maybeWarnOthers( Selector sel, SelOpt, Selected, Others, wibble::Preferred ) ->
    typename std::enable_if< SelOpt::value & SE_WarnOther >::type
{
    std::vector< Atom > others;
    fillOthers( Selector( sel ), others, Others() ); // we need copy of selector here
    if ( (SelOpt::value & SE_LastDefault) != 0 ) {
        assert_leq( 1, others.size() ); // there should be at least default
        others.pop_back(); // remove default -- as it should be here every time
    }
    if ( others.size() )
        sel.warnOtherAvailable( Atom( Selected() ), others );
}

template< typename Selector, typename SelOpt, typename Selected, typename Others >
auto maybeWarnOthers( Selector sel, SelOpt, Selected, Others, wibble::NotPreferred ) ->
    void
{ }

// selects current component of instantiation chain if its prerequisities are satisfied
template< typename Selector, typename SelOpt, typename Head, typename Tail, typename Default,
    typename ToSelect, typename Selected >
auto runIfValid( Selector &sel, SelOpt, Head, Tail, Default, ToSelect, Selected,
        wibble::Preferred, wibble::Preferred )
    -> typename std::enable_if< std::is_base_of< InstantiationError, Head >::value,
        typename Selector::ReturnType >::type
{
    return runOne( sel, SelOpt(), Tail(), Default(), ToSelect(), Selected() );
}

template< typename Selector, typename SelOpt, typename Head, typename Tail, typename Default,
    typename ToSelect, typename Selected >
auto runIfValid( Selector &sel, SelOpt, Head, Tail, Default, ToSelect, Selected,
        wibble::Preferred, wibble::NotPreferred )
    -> typename std::enable_if<
        ! std::is_base_of< InstantiationError, Head >::value
        && EvalBoolExpr< ContainsP< Selected >::template Elem,
            typename Head::SupportedBy >::value,
        typename Selector::ReturnType >::type
{
    return sel.ifSelect( Head(), [ &sel ]() -> typename Selector::ReturnType {
            maybeWarnOthers( sel, SelOpt(), Head(), Tail(), wibble::Preferred() );
            return runSelector( sel, ToSelect(),
                typename Append< Head, Selected >::T() );
        }, [ &sel ]() {
            return runOne( sel, SelOpt(), Tail(), Default(), ToSelect(), Selected() );
        } );
}

template< typename Selector, typename SelOpt, typename Head, typename Tail, typename Default,
    typename ToSelect, typename Selected >
auto runIfValid( Selector &sel, SelOpt, Head, Tail, Default, ToSelect, Selected,
        wibble::NotPreferred, wibble::NotPreferred )
    -> typename Selector::ReturnType
{
    return runOne( sel, SelOpt(), Tail(), Default(), ToSelect(), Selected() );
}

// in case something goes wrong and there is nothing to select
template< typename Selector, typename SelOpt, typename Default >
auto instantiationError( Selector &sel, SelOpt, Default, wibble::Preferred ) ->
    typename std::enable_if< (SelOpt::value & SE_LastDefault ) == 0,
        typename Selector::ReturnType >::type
{
    return sel.instantiationError( Atom( Default() ).component );
}

template< typename Selector, typename SelOpt, typename Default >
auto instantiationError( Selector &sel, SelOpt, Default, wibble::NotPreferred ) ->
    typename Selector::ReturnType
{
    return sel.instantiationError( Default() );
}

// select default if allowed (default is alwais last of list)
template< typename Selector, typename SelOpt, typename Default, typename ToSelect, typename Selected >
auto selectDefault( Selector &sel, SelOpt, Default, ToSelect, Selected, wibble::Preferred )
    -> typename std::enable_if<
            std::is_base_of< InstantiationError, Default >::value
            || (SelOpt::value & SE_LastDefault) == 0,
        typename Selector::ReturnType >::type
{
    return instantiationError( sel, SelOpt(), Default(), wibble::Preferred() );
}

template< typename Selector, typename SelOpt, typename Default, typename ToSelect, typename Selected >
auto selectDefault( Selector &sel, SelOpt, Default, ToSelect, Selected, wibble::NotPreferred ) ->
    typename Selector::ReturnType
{
    return runSelector( sel, ToSelect(),
            typename Append< Default, Selected >::T() );
}

// select one component
template< typename Selector, typename SelOpt, typename Current, typename Default,
    typename ToSelect, typename Selected >
auto runOne( Selector &sel, SelOpt, Current, Default, ToSelect, Selected )
    -> typename std::enable_if< ( Current::length > 0 ),
        typename Selector::ReturnType >::type
{
    using Head = typename Current::Head;
    using Tail = typename Current::Tail;

    return runIfValid( sel, SelOpt(), Head(), Tail(), Default(), ToSelect(), Selected(),
            wibble::Preferred(), wibble::Preferred() );
}

template< typename Selector, typename SelOpt, typename Current, typename Default,
    typename ToSelect, typename Selected >
auto runOne( Selector &sel, SelOpt, Current, Default, ToSelect, Selected )
    -> typename std::enable_if< Current::length == 0, typename Selector::ReturnType >::type
{
    return selectDefault( sel, SelOpt(), Default(), ToSelect(), Selected(), wibble::Preferred() );
}

// the main entry point (externally called as runSelector( selectore, toSelect, TypeList<>() )
template< typename Selector, typename ToSelect, typename Selected >
auto runSelector( Selector &sel, ToSelect, Selected )
    -> typename Selector::ReturnType
{
    using Head = typename NotMissing< typename ToSelect::Head::List >::T;
    using SelOpt = std::integral_constant< SelectOption, ToSelect::Head::opts >;
    using Default = typename Default< Head >::T;
    using Tail = typename ToSelect::Tail;
    return runOne( sel, SelOpt(), Head(), Default(), Tail(), Selected() );
}

// everything is done
template< typename Selector, typename Selected >
auto runSelector( Selector &sel, TypeList<>, Selected )
    -> typename Selector::ReturnType
{
    return sel.template create< typename Reverse< Selected >::T >();
}

}
}

#endif // DIVINE_INSTANCES_SELECT_IMPL
