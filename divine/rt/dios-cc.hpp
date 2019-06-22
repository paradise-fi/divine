#pragma once

#include <divine/cc/driver.hpp>
#include <divine/cc/link.hpp>
#include <divine/cc/native.hpp>
#include <divine/rt/runtime.hpp>

namespace divine {
namespace rt {

struct DiosCC : cc::Driver
{
    using Options = cc::Options;

    // set in .cpp
    static const std::vector< std::string > defaultDIVINELibs;

    explicit DiosCC( std::shared_ptr< llvm::LLVMContext > ctx ) : Driver( Options(), ctx )
    {
        setupFS( rt::each );
    }
    explicit DiosCC( Options opts = Options(),
                         std::shared_ptr< llvm::LLVMContext > ctx = nullptr );
    DiosCC( DiosCC&& ) = default;
    DiosCC& operator=( DiosCC&& ) = default;

    void setup( Options opts ) { this->opts = opts; }

    void link_dios_config( std::string cfg );
    void build( cc::ParsedOpts po );
};

void add_dios_header_paths( std::vector< std::string >& paths );

struct NativeDiosCC : cc::Native
{
    NativeDiosCC( const std::vector< std::string >& opts );

    auto link_dios_native( bool cxx );
    std::unique_ptr< llvm::Module > link_bitcode() override;
    void link() override;
    void set_cxx( bool cxx ) { _cxx = cxx; }

    bool _cxx;
};

} // rt
} // divine
