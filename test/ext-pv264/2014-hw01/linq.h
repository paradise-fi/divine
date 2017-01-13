#include <vector>
#include <type_traits>
#include <algorithm>
#include <cassert>
#include <set>

#ifndef QUERY_H
#define QUERY_H

namespace query {

/* Implementujte tridu Vector< T >, ktera se chova stejne jako
 * std::vector< T > a navic ma dotazovaci funkce popsane nize.
 *
 * U nekterych funkci je uvedena slozitost v poctu nekterych konkretnich
 * operaci, slozitost v poctu neuvedenych operaci muze byt libovolna
 * uvedena slozitost je vzdy maximalni pripustna hodnota n oznacuje
 * delku vektory
 */

template< typename T >
struct Vector : std::vector< T > {
    using std::vector< T >::vector;

    // Ocekava binarni funkci BinaryFn volatelnou jako R( R &, const T & )
    // aplikuje BinaryFn postupne na vchechny prvky vektoru za pouziti seed
    // jako pocatecni hodnoty:
    // necht x1, x2 ... xn jsou prvky vektoru, pak vyledek accumulate
    // je fn( ... fn( fn( seed, x1 ), x2 ) ..., xn )
    // slozitost: presne n volani BinaryFn
    template< typename R, typename BinaryFn >
    R accumulate( R seed, BinaryFn fn ) const {
        for ( const auto &x : *this )
            seed = fn( seed, x );
        return seed;
    }

    // spocita soucet vsech prvku vektoru
    T sum() const {
        return accumulate( T(), []( const T &x, const T &y ) { return x + y; } );
    }

    // spocita prumer vsech prvku vektoru
    T average() const {
        return sum() / this->size();
    }

    // spocita median vsech prvku vektoru
    T median() const {
        assert( this->size() >= 1 );
        return sort()[ this->size() / 2 ];
    }

    // aplikuje funkci UnaryFn ~~ X( const T & ) na kazdy prvek vektoru,
    // jeji navratovou hodnotu ignoruje
    // slozitost: presne n volani UnaryFn
    template< typename UnaryFn >
    void forEach( UnaryFn fn ) const {
        for ( const auto &x : *this )
            fn( x );
    }

    // aplykuje funkci UnaryFn ~~ X( const T & ) na kazdy prvek vektoru
    // a vysledky vlozi v poradi do vysledneho vektoru
    // slozitost: presne n volani UnaryFn + 1 resize vektoru
    template< typename UnaryFn >
    auto map( UnaryFn fn ) const -> Vector< typename std::result_of< UnaryFn( T ) >::type > {
        Vector< typename std::result_of< UnaryFn( T ) >::type > out;
        out.reserve( this->size() );
        for ( const auto &x : *this )
            out.emplace_back( fn( x ) );
        return out;
    }

    // vytvori vektor, ktery obsahuje ty prvky z this, ktere splnuji
    // predikat UnaryPred ~~ bool( const T & )
    // slozitos: presne n volani UnaryPred
    template< typename UnaryPred >
    Vector< T > filter( UnaryPred p ) const {
        Vector< T > out;
        for ( const auto &x : *this )
            if ( p( x ) )
                out.emplace_back( x );
        return out;
    }

    // vrati serazeny vektor se stejnymy prvky jako this
    // slozitost: O( n log n ) porovnani
    Vector< T > sort() const {
        Vector< T > out = *this;
        std::sort( out.begin(), out.end() );
        return out;
    }

    // seradi vektor za pomoci dodane porovnavaci funkce
    // Compare ~~ bool( const T &, const T & ), ktera se pouzije namisto
    // operator<
    // slozitost: O( n log n ) porovnani
    template< typename Compare >
    Vector< T > sortBy( Compare compare ) const {
        Vector< T > out = *this;
        std::sort( out.begin(), out.end(), compare );
        return out;
    }

    // obrati poradi prvku ve vektoru
    // slozitos: n vlozeni + 1 resize
    Vector< T > reverse() const {
        Vector< T > out;
        out.reserve( this->size() );
        std::copy( this->rbegin(), this->rend(), std::back_inserter( out ) );
        return out;
    }

    // vybere z this unikatni prvky tak, ze zachova vzdy prvni z opakovane
    // se vyskytujicich prvku a poradi unitatnich prvku ve vystupnim vektoru
    // bude stejne jejich poradi ve vstupnim
    // slozitost: O( n log n ) porovnani prvku
    Vector< T > unique() const {
        std::set< T > st;
        return filter( [&st]( const T &x ) { return st.emplace( x ).second; } );
    }


    // pro vektor kontejneru vrati vektor predstavujici zretezeni vsech prvku
    // vnitrnich konteineru
    template< typename Inner >
    Vector< Inner > flatten() const {
        Vector< Inner > out;
        for ( const auto &nested : *this )
            for ( const auto &x : nested )
                out.emplace_back( x );
        return out;
    }
};

}

#endif // QUERY_H
