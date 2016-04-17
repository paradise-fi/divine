// -*- C++ -*- (c) 2016 Vladimír Štill

#include "cc.h"

int main() {
    divine::cc::Compiler c;
    c.mapVirtualFile( "test.c",
"#include \"include/bla.h\"\n"
"union X { int i; char c[ sizeof( int ) ]; };\n"
"int main() {\n"
"   union X x;\n"
"   x.c[0] = 42;\n"
"   return x.i;\n"
"}"
        );
    c.mapVirtualFile( "include/bla.h", "#warning bla" );
    auto m = c.compileModule( "test.c" );
    m->dump();
}
