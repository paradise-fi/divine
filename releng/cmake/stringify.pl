use strict;

my $ns = shift @ARGV;
my $name = shift @ARGV;
my $in = shift @ARGV;

my $out = $name;
$out =~ s,[./+-],_,g;
local $/;
open IN, $in;
my $data = <IN>;
my $len = length $data;

$data =~ s:([^a-zA-Z0-9 {}()<>.#+*/_',;=^-]):"\\x" . sprintf("%02x", ord($1)):ge;
$data =~ s:\\x0a:\\x0a"\n":gsm;
$data =~ s:(\\x[0-9a-f]{2})([a-fA-F0-9]):$1""$2:g;

open OUT, ">str_${out}.cpp";
print OUT "#include <string_view>\n";
print OUT "namespace divine::str::${ns} {\n";
print OUT "extern const std::string_view ${name}( \n";
print OUT '"';
print OUT $data;
print OUT "\", $len ); }";
close OUT;
