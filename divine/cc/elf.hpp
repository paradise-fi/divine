// -*- C++ -*- (c) 2016 Vladimír Štill
#pragma once

#include <divine/cc/clang.hpp>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

#include <brick-except>
#include <string>
#include <vector>

namespace divine {
namespace cc {
namespace elf {

struct ElfError : brick::except::Error
{
    using brick::except::Error::Error;
};

std::vector< std::string > getSections( std::string filepath, std::string sectionName );

void linkObjects( std::string output, std::vector< std::string > args );

std::vector< std::unique_ptr< llvm::Module > > extractModules( std::string file, llvm::LLVMContext &ctx );

void compile( Compiler &cc, std::string path, std::string output, std::vector< std::string > args );

std::tuple< std::vector< std::string >, std::vector< std::string > >
    getExecLinkOptions( std::vector< std::string > divineSearchPaths );

}
}
}
