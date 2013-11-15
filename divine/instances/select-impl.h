// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>

#include <type_traits>
#include <vector>
#include <cctype>

#include <wibble/sfinae.h>

#include <divine/instances/definitions.h>

#ifndef DIVINE_INSTANCES_SELECT_IMPL
#define DIVINE_INSTANCES_SELECT_IMPL

namespace divine {
namespace instantiate {

template< typename T >
struct IsNotMissing {
    static constexpr bool value = !std::is_same< T, _Missing >::value;
};

template< typename List >
using NotMissing = typename Filter< IsNotMissing, List >::T;

template< typename Selected >
struct Avail1 {
    template< typename X >
    struct A {
        static constexpr bool value =
            EvalBoolExpr< ContainsP< Selected >::template Elem,
                          typename X::SupportedBy >::value;
    };
};

template< typename List, typename Selected >
using Available = typename Filter< Avail1< Selected >::template A, List >::T;

/*****************************************************************************/


// test what could have been selected (note that we work with copy of selector!)
template< typename Selector, typename... Others >
inline std::vector< Atom > getOthers( Selector sel, Others... others ) {
    Selectable *ot[ sizeof...( Others ) ] = { &others... };
    std::vector< Atom > ret;
    for ( int i = 0; i < sizeof...( Others ); ++i )
        if ( sel.trySelect( *ot[ i ] ) )
            ret.push_back( ot[ i ]->atom() );
    return ret;
}

// find out what would be selected and warn about it if that was requested
template< typename Selector, typename SelOpt, typename Selected, typename... Others >
inline auto maybeWarnOthers( Selector sel, SelOpt, Selected, wibble::Preferred, TypeList< Others... > ) ->
    typename std::enable_if< SelOpt::value & SE_WarnOther >::type
{
    // we need copy of selector here
    std::vector< Atom > others = getOthers( Selector( sel ), Others()... );
    if ( others.size() && (SelOpt::value & SE_LastDefault) != 0 ) {
        others.pop_back(); // remove default -- as it should be here every time
    }
    if ( others.size() )
        sel.warnOtherAvailable( Selected().atom(), others );
}

template< typename Selector, typename SelOpt, typename Selected, typename Others >
inline void maybeWarnOthers( Selector sel, SelOpt, Selected, wibble::NotPreferred, Others )
{ }

// in case something goes wrong and there is nothing to select
template< typename Selector, typename SelOpt >
inline auto instantiationError( Selector &sel, SelOpt, wibble::Unit, wibble::Preferred ) ->
    typename Selector::ReturnType
{
    return sel.instantiationError( std::string( "<no default available>" ) );
}

template< typename Selector, typename SelOpt, typename Default >
inline auto instantiationError( Selector &sel, SelOpt, Default, wibble::NotPreferred ) ->
    typename Selector::ReturnType
{
    return sel.instantiationError( Default().atom().component );
}

// select default if allowed (default is alwais last of list)
template< typename Selector, typename SelOpt, typename Default, typename ToSelect, typename Selected >
inline auto selectDefault( Selector &sel, SelOpt, Default, ToSelect, Selected, wibble::Preferred )
    -> typename std::enable_if<
            std::is_same< Default, wibble::Unit >::value
            || (SelOpt::value & SE_LastDefault) == 0,
        typename Selector::ReturnType >::type
{
    return instantiationError( sel, SelOpt(), Default(), wibble::Preferred() );
}

template< typename Selector, typename SelOpt, typename Default, typename ToSelect, typename Selected >
inline auto selectDefault( Selector &sel, SelOpt, Default, ToSelect, Selected, wibble::NotPreferred ) ->
    typename Selector::ReturnType
{
    return runSelector( sel, ToSelect(),
            typename Append< Default, Selected >::T() );
}

template< typename Selector, typename SelOpt, typename Current, typename Default,
    typename NewDefault, typename ToSelect, typename Selected >
inline auto addNewDefaultAndRun( Selector &sel, SelOpt, Current, Default, NewDefault, ToSelect, Selected )
    -> typename std::enable_if< (SelOpt::value & SE_LastDefault) != 0,
        typename Selector::ReturnType >::type
{
    return runOne( sel, SelOpt(), Current(), NewDefault(), ToSelect(), Selected() );
}

template< typename Selector, typename SelOpt, typename Current, typename Default,
    typename NewDefault, typename ToSelect, typename Selected >
inline auto addNewDefaultAndRun( Selector &sel, SelOpt, Current, Default, NewDefault, ToSelect, Selected )
    -> typename std::enable_if< (SelOpt::value & SE_LastDefault) == 0,
        typename Selector::ReturnType >::type
{
    return runOne( sel, SelOpt(), Current(), Default(), ToSelect(), Selected() );
}

// select one component
template< typename Selector, typename SelOpt, typename Current, typename Default,
    typename ToSelect, typename Selected >
inline auto runOne( Selector &sel, SelOpt, Current, Default, ToSelect, Selected )
    -> typename std::enable_if< ( Current::length > 0 ),
        typename Selector::ReturnType >::type
{
    using Head = typename Current::Head;
    using Tail = typename Current::Tail;

    return sel.ifSelect( Head(), (SelOpt::value & SE_WarnUnavailable) != 0,
        [ &sel ]() -> typename Selector::ReturnType {
            maybeWarnOthers( sel, SelOpt(), Head(), wibble::Preferred(), Tail() );
            return runSelector( sel, ToSelect(),
                typename Append< Head, Selected >::T() );
        }, [ &sel ]() { // not selected but available
            return addNewDefaultAndRun( sel, SelOpt(), Tail(), Default(), Head(), ToSelect(), Selected() );
        }, [ &sel ]() { // not available
            return runOne( sel, SelOpt(), Tail(), Default(), ToSelect(), Selected() );
        } );
}

template< typename Selector, typename SelOpt, typename Current, typename Default,
    typename ToSelect, typename Selected >
inline auto runOne( Selector &sel, SelOpt, Current, Default, ToSelect, Selected )
    -> typename std::enable_if< Current::length == 0, typename Selector::ReturnType >::type
{
    return selectDefault( sel, SelOpt(), Default(), ToSelect(), Selected(), wibble::Preferred() );
}

// the main entry point (externally called as runSelector( selectore, toSelect, TypeList<>() )
template< typename Selector, typename ToSelect, typename Selected >
auto runSelector( Selector &sel, ToSelect, Selected )
    -> typename Selector::ReturnType
{
    using Head_0 = NotMissing< typename ToSelect::Head::List >;
    using Head = Available< Head_0, Selected >;
    using SelOpt = std::integral_constant< SelectOption, ToSelect::Head::opts >;
    using Tail = typename ToSelect::Tail;
    return runOne( sel, SelOpt(), Head(), wibble::Unit(), Tail(), Selected() );
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
