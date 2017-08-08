// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <lart/support/pass.h>
#include <lart/support/meta.h>

namespace lart {
namespace abstract {

struct AddAssumes {

	static PassMeta meta() {
    	return passMeta< AddAssumes >( "AddAssumes", "Add assumes to controlflow of program." );
    }

	void run( llvm::Module &m );

    void process( llvm::Instruction * inst );
};

static inline PassMeta assume_pass() {
    return AddAssumes::meta();
}

}
}
