#include <iostream>

#ifndef LART_SUPPORT_META_H
#define LART_SUPPORT_META_H

#include <string>
#include <sstream>
#include <iostream>
#include <llvm/IR/PassManager.h>

namespace lart {

struct PassMeta {

    explicit PassMeta( std::string name, std::string description = "",
            std::function< void ( llvm::ModulePassManager &, std::string ) > create = nullptr,
            std::vector< std::shared_ptr< PassMeta > > subpasses = { } ) :
        _name( name ), _description( description ), _create( create ),
        _subpasses( subpasses )
    { }

    std::string name() { return _name; }
    auto subpasses() { return _subpasses; }

    void description( std::ostream &os, int level = 0 ) {
        std::string indent = "";
        for ( int i = 0; i < level; ++ i )
            indent += "    ";

        if ( level == 0 )
            os << "Pass " << _name << std::endl;
        else
            os << indent << "Subpass " << _name << std::endl;
        if ( !_description.empty() ) {
            _indent( indent + "    ", _description, os );
            os << std::endl;
        }
        for ( auto sub : subpasses() )
            sub->description( os, level + 1 );
    }

    void create( llvm::ModulePassManager &mgr, std::string opt ) {
        if ( _create )
            _create( mgr, opt );
        else
            defCreate( mgr, opt );
    }

    void defCreate( llvm::ModulePassManager &mgr, std::string opt ) {
        ASSERT( opt.empty() );
        ASSERT( !_subpasses.empty() );
        for ( auto &s : _subpasses )
            s->create( mgr, "" );
    }


    bool select( llvm::ModulePassManager &mgr, std::string selector, std::string opt ) {
        if ( _name == selector ) {
            create( mgr, opt );
            return true;
        }
        return false;
    }

  private:
    std::string _name;
    std::string _description;
    std::function< void ( llvm::ModulePassManager &, std::string ) > _create;
    std::vector< std::shared_ptr< PassMeta > > _subpasses;

    void _indent( std::string indent, std::string text, std::ostream &os ) {
        std::stringstream ss( text );
        std::string line;
        while ( std::getline( ss, line ) ) {
            os << indent << line << std::endl;
        }
    }

};

template< typename... Ps >
std::vector< std::shared_ptr< PassMeta > > passVector() {
    return { std::make_shared< PassMeta >( Ps::meta() )... };
}

template< typename... Subs >
PassMeta passMetaC( std::string name, std::string description = "",
            std::function< void ( llvm::ModulePassManager &, std::string ) > create = nullptr )
{
    return PassMeta( name, description, create, passVector< Subs... >() );
}

template< typename Self, typename... Subs >
PassMeta passMeta( std::string name, std::string description = "" ) {

    return passMetaC< Subs... >( name, description,
            []( llvm::ModulePassManager &mgr, std::string ) { mgr.addPass( Self() ); } );
}

template< typename... Subs >
PassMeta compositePassMeta( std::string name, std::string description = "" ) {
    return passMetaC< Subs... >( name, description );
}

}

#endif // LART_SUPPORT_META_H
