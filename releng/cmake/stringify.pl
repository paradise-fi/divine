use strict;

sub flatten
{
    local($_) = @_;
    s|\.|_|g;
    s|/|_|g;
    s|-|_|g;
    s|\+|_|g;
    s|~|_|g;
    return $_;
}

my $ns = shift @ARGV;
my ( $NSBEG, $NSEND, $NSPRE );

if ( $ns )
{
    $ns = "src_${ns}";
    $NSBEG = "namespace ${ns} {";
    $NSEND = "}";
    $NSPRE = "${ns}::";
}

if ( $ARGV[0] eq "-l" )
{
    shift @ARGV;
    my $var = (shift @ARGV) . "_list";
    my $out = "$var.cpp";
    unlink( $out );
    open OUT, ">$out";

    print OUT "#include <string>\n";
    print OUT "namespace divine {\n";
    print OUT "$NSBEG\n";
    for (@ARGV)
    {
        my $name = flatten($_);
        print OUT "extern const std::string ${name}_str;\n";
        # print OUT "extern const int ${name}_len;\n";
    }
    print OUT "$NSEND";

    print OUT "struct stringtable { std::string n; const std::string &c; };";
    print OUT "stringtable ${var}"."[] = { ";

    for (@ARGV)
    {
        my $name = ${NSPRE} . flatten($_);
        print OUT "{ \"$_\", ${name}_str },\n";
    }
    print OUT '{ "", "" }' . "\n};\n}";
}
else
{
    my $name = flatten($ARGV[1]);
    local $/;
    open IN, "$ARGV[0]/$ARGV[1]";
    my $data = <IN>;
    my $len = length $data;

    open OUT, ">${name}_str.cpp";
    print OUT "#include <string>\n";
    print OUT "namespace divine{\n${NSBEG}\n";
    print OUT "extern const std::string ${name}_str( \n";
    print OUT '"';
    $data =~ s:([^a-zA-Z0-9 {}()<>.#+*/_',;=^-]):"\\x" . sprintf("%02x", ord($1)):ge;
    $data =~ s:\\x0a:\\x0a"\n":gsm;
    $data =~ s:(\\x[0-9a-f]{2})([a-fA-F0-9]):$1""$2:g;
    print OUT $data;
    print OUT "\", $len ); } ${NSEND}";
    close OUT;
}
