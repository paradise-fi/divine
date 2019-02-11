// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/synthesize.h>

namespace lart::abstract
{
    void Synthesize::run( llvm::Module & /*m*/ ) { UNREACHABLE( "Not implemented" ); }
    void Synthesize::process( const Taint & /*taint*/ ) { UNREACHABLE( "Not implemented" ); }
} // namespace lart::abstract
