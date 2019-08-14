// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2017 Jan Horáček <me@apophis.cz>
 * (c) 2017-2019 Zuzana Baranová <xbaranov@fi.muni.cz>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <divine/cc/cc1.hpp>
#include <divine/cc/codegen.hpp>

DIVINE_RELAX_WARNINGS
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCSection.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Support/TargetRegistry.h"
DIVINE_UNRELAX_WARNINGS

#include <brick-llvm>
using namespace llvm;

namespace divine::cc
{
    struct PM_BC : legacy::PassManager
    {
        MCStreamer* mc = nullptr;

        void add( Pass *P ) override
        {
            legacy::PassManager::add( P );

            if( auto printer = dynamic_cast< AsmPrinter* >( P ) )
                mc = printer->OutStreamer.get();
        }
    };

    // Create an object file out of the specified module.
    // This also creates the .llvmbc section inside the binary
    // which includes the serialized module bitcode.
    int emit_obj_file( Module &m, std::string filename, bool pic /* = false */ )
    {
        //auto TargetTriple = sys::getDefaultTargetTriple();
        auto TargetTriple = "x86_64-unknown-none-elf";

        divine::cc::initTargets();

        std::string Error;
        auto Target = TargetRegistry::lookupTarget( TargetTriple, Error );

        // Print an error and exit if we couldn't find the requested target.
        // This generally occurs if we've forgotten to initialise the
        // TargetRegistry or we have a bogus target triple.
        if ( !Target )
        {
            errs() << Error;
            return 1;
        }

        auto CPU = "generic";
        auto Features = "";

        TargetOptions opt;
        auto RM = pic ? Reloc::PIC_ : Reloc::Static;

        using TargetPtr = std::unique_ptr< llvm::TargetMachine >;
        TargetPtr tmach{ Target->createTargetMachine( TargetTriple, CPU,
                                                      Features, opt, RM ) };

        m.setDataLayout( tmach->createDataLayout() );
        m.setTargetTriple( TargetTriple );

        std::error_code EC;
        raw_fd_ostream dest( filename, EC, sys::fs::F_None );

        if ( EC )
        {
            errs() << "Could not open file: " << EC.message();
            return 1;
        }

        PM_BC PM;

        if ( tmach->addPassesToEmitFile( PM, dest, nullptr,
                                         TargetMachine::CGFT_ObjectFile, false ) )
        {
            errs() << "TargetMachine can't emit a file of this type\n";
            return 1;
        }

        MCStreamer *AsmStreamer = PM.mc;
        // Write bitcode into section .llvmbc
        AsmStreamer->SwitchSection( AsmStreamer->getContext().getELFSection( llvm_section_name,
                                                                             ELF::SHT_NOTE, 0 ) );
        std::string bytes = brick::llvm::getModuleBytes( &m );
        AsmStreamer->EmitBytes( bytes );

        PM.run( m );
        dest.flush();

        return 0;
    }
}
