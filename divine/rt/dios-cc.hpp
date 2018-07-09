#pragma once

#include <divine/cc/driver.hpp>
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

    void setup( Options opts ){ opts = opts; }

    void linkEssentials();
    void runCC ( std::vector< std::string > rawCCOpts,
                 std::function< ModulePtr( ModulePtr &&, std::string ) > moduleCallback = nullptr ) override;
};

} // rt
} // divine
