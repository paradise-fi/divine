#!/usr/bin/env perl

my $collapse = ($ARGV[0] =~ /collapse/);

print "digraph G {";
my $cluster = "";

sub want() {
    my ( $x ) = @_;
    return $x =~ /^divine|^tools/ && $x !~ /legacy|test\.h/;
}

sub dir() {
    my ( $dir ) = @_;
    $dir =~ s,^(.*)/[^/]*$,$1,;
    $dir =~ s,/,_,g;
    return $dir;
};

my @files = grep { &want($_) }
    map { $_ =~ s,^\./,,; $_ } grep /\.cc$|\.cpp$|\.h$|\.hh$/,
        (split /\0/,` find -type f -print0`);
        

for my $f (@files) {
    my $dir = &dir($f);

    next unless (&want($f));

    unless ($collapse or $dir eq $cluster and not $dir eq "divine") {
        print "}\n" if $cluster;
        print "subgraph \"cluster_$dir\" {\n";
        print "label = \"$dir\";\n";
        print "style = filled;\n";
        print "color = lightblue;\n";
        $cluster = $dir;
    };

    if ($dir eq "divine" or not $collapse) {
        print "\"$f\"";
        print "[color=grey,fontcolor=grey]" if ($f =~ /\.cpp$|\.cc$/);
        print "\n;";
    }
}

print "}\n" unless ($collapse);

my %seen;

for my $f (@files) {
    my @l = grep /^#include </, (split /\n/, `cat $f`);
    for (@l) {
        s/^#include <(.*)>.*$/$1/;
        my $a = $f;
        my $b = $_;

        next unless (&want($a) and &want($b));

        if ($collapse) {
            $a = &dir($f) unless (&dir($f) eq "divine");;
            $b = &dir($_) unless (&dir($_) eq "divine");
            next if ($a eq $b);
            next if $seen{"$a -> $b"};
            $seen{"$a -> $b"} = 1;
        }

        print "\"$a\" -> \"$b\"";

        print "[color=gray]" if (&dir($a) eq &dir($b));
        print "[color=red]" if ($a =~ /toolkit/ && $b !~ /toolkit/);
        print "[color=red]" if ($a =~ /utility/ && $b !~ /utility/);
        print ";\n";
    }
};

print "}";
