use strict;

my $ns = shift @ARGV;
my $var = (shift @ARGV) . "_list";
my $out = "$var.cpp";

sub varname { local $_ = shift; s,[.+/-],_,g; return $_; }

unlink( $out );
open OUT, ">$out";

print OUT "#include <string_view>\n#include <string>\n";
print OUT "namespace divine::str::${ns}\n{\n";
print OUT "extern const std::string_view " . varname($_) . ";\n" for ( @ARGV );
print OUT "}\n\n";
print OUT "namespace divine::str\n{\n";
print OUT "struct stringtable { std::string n; std::string_view c; };\n";
print OUT "stringtable ${var}"."[] = { \n";
print OUT "  { \"$_\", ${ns}::" . varname($_) . " },\n" for ( @ARGV );
print OUT '  { "", "" }' . "\n};\n";
print OUT "}\n";
