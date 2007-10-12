#!/usr/bin/perl

my $in, $set, %tests, %prefix;

for $file (@ARGV) {
    $in = `cat $file`;

# TODO we should push/pop on { and }
    for (split /[;{}]/, $in) {
        #print "parsing: $_\n";
        if (/struct _?([tT]est_?)([A-Za-z]+)/) {
            #push @sets, $1;
            $set = $2;
            $prefix{$set} = $1;
        } elsif (/Test[ \t\n]+([a-zA-Z]+)[ \t\n]*\(/) {
            $test{$set} = () unless ($test{$set});
            push @{$tests{$set}}, $1;
        }
    }
}
    
print "#include <wibble/test.h>\n";
for $file (@ARGV) {
    print "#include \"$file\"\n"
}
print "#include <wibble/test-runner.h>\n";

for (keys %tests) {
    my $set = $_;
    for (@{$tests{$set}}) {
        print "void run$set$_() {";
        print " RUN( $prefix{$set}$set, $_ ); }\n";
    }

    print "RunTest run${set}[] = {\n";
    print "\t{ \"$_\", run$set$_ },\n" for (@{$tests{$set}});
    print "};\n";
}

print "RunSuite suites[] = {\n";
for (keys %tests) {
    my $count = scalar @{$tests{$_}};
    print "{ \"$_\", run$_, $count },\n";
}
print "};\n";
print "#include <wibble/test-main.h>\n";
