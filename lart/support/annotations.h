// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>

#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <string>
#include <sstream>
#include <initializer_list>
#include <algorithm>
#include <brick-types>

#ifndef LART_ANNOTATIONS_H
#define LART_ANNOTATIONS_H

namespace lart {
namespace annos {

struct Annotation : brick::types::Comparable {
    Annotation() = default;
    explicit Annotation( std::string anno ) {
        size_t oldoff = 0, off = 0;
        do {
            off = anno.find( oldoff, '.' );
            _parts.emplace_back( anno.substr( oldoff, off - oldoff) );
            oldoff = off + 1;
        } while ( off != std::string::npos );
    }
    template< typename It >
    Annotation( It begin, It end ) : _parts( begin, end ) { }
    Annotation( std::initializer_list< std::string > parts ) : _parts( parts ) { }

    std::string name() { return _parts.back(); }
    Annotation ns() { return Annotation( _parts.begin(), _parts.end() - 1); }
    std::string toString() {
        std::stringstream ss;
        for ( auto &n : _parts )
            ss << n << ".";
        auto str = ss.str();
        return str.substr( 0, str.size() - 1 );
    }

    bool inNamespace( Annotation ns ) {
        return ns._parts.size() < _parts.size()
            && std::equal( ns._parts.begin(), ns._parts.end(), _parts.begin() );
    }

    Annotation dropNamespace( Annotation ns ) {
        return inNamespace( ns )
             ? Annotation( _parts.begin() + ns.size(), _parts.end() )
             : *this;
    }

    size_t size() const { return _parts.size(); }

    bool operator==( const Annotation &o ) const {
        return o.size() == size() && std::equal( _parts.begin(), _parts.end(), o._parts.begin() );
    }

  private:
    std::vector< std::string > _parts;
};

template< typename Yield >
void enumerateFunctionAnnos( llvm::Module &m, Yield yield ) {
    auto annos = m.getNamedGlobal( "llvm.global.annotations" );
    if ( !annos )
        return;
    auto a = llvm::cast< llvm::ConstantArray >( annos->getOperand(0) );
    for ( int i = 0; i < int( a->getNumOperands() ); i++ ) {
        auto e = llvm::cast< llvm::ConstantStruct >( a->getOperand(i) );
        if ( auto fn = llvm::dyn_cast< llvm::Function >( e->getOperand(0)->getOperand(0) ) ) {
            std::string anno = llvm::cast< llvm::ConstantDataArray >(
                        llvm::cast< llvm::GlobalVariable >( e->getOperand(1)->getOperand(0) )->getOperand(0)
                    )->getAsCString();
            yield( fn, Annotation( anno ) );
        }
    }
}

template< typename Yield >
void enumerateFunctionAnnosInNs( Annotation ns, llvm::Module &m, Yield yield ) {
    enumerateFunctionAnnos( m, [&]( llvm::Function *fn, Annotation anno ) {
            if ( anno.inNamespace( ns ) )
                yield( fn, anno.dropNamespace( ns ) );
        } );
}

template< typename Yield >
void enumerateFunctionsForAnno( Annotation anno, llvm::Module &m, Yield yield ) {
    enumerateFunctionAnnos( m, [&]( llvm::Function *fn, Annotation a ) {
            if ( a == anno )
                yield( fn );
        } );
}

template< typename Yield >
void enumerateFunctionsForAnno( std::string anno, llvm::Module &m, Yield yield ) {
    enumerateFunctionsForAnno( Annotation( anno ), m, yield );
}

}
}

#endif // LART_ANNOTATIONS_H
