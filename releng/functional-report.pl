#!/usr/bin/env perl

use warnings;
use strict;
use File::Spec::Functions;

die "usage: functional-report.pl <DIRECTORY_WITH_RESULTS> <WEBSITE_SRC_DIR>" if ( @ARGV < 2 );

my $dir = $ARGV[0];
my $webdir = $ARGV[1];
my $journal = catfile( $dir, "journal" );
my $list = catfile( $dir, "list" );
my $out = catfile( $dir, "html" );

my @flavours;
my @tests;
my %seenFlavours;
my %seenTests;
my %results;
my %rescount;

open( my $listhandle, "<", $journal );
while ( <$listhandle> ) {
    chomp;
    m/^([^:]*):([^\s]*)\s+(.*)$/;

    my ( $fl, $test, $status ) = ( $1, $2, $3 );
    my $id = "$fl:$test";

    unless ( exists $seenFlavours{ $fl } ) {
        $seenFlavours{ $fl } = 1;
        push( @flavours, $fl );
    }
    unless ( exists $seenTests{ $test } ) {
        $seenTests{ $test } = 1;
        push( @tests, $test );
    }
    $rescount{ $status } = 0 unless ( exists $rescount{ $status } );
    $rescount{ $status }++;

    $results{ $id } = $status;
}
close( $listhandle );

delete $rescount{ started };

mkdir( $out );

open( my $index, ">", catfile( $out, "index.md" ) );

my $summary = "";

for my $k ( sort keys %rescount ) {
    $summary .= "<span class=\"test_$k\">$rescount{ $k } $k</span>, ";
}
$summary =~ s/, $//;

my $builddate = "unknown";
my $buildtype = "unknown";
my $divine = "unknown";

my $divineExec = catfile( $dir, "..", "..", "tools", "divine" );
if ( -x $divineExec ) {
    open( my $dv, "$divineExec --version 2>/dev/null |" );
    while ( <$dv> ) {
        chomp;
        if ( /^version: +(.*)$/ ) {
            $divine = $1;
        }
        if ( /^build date: +(.*)/ ) {
            $builddate = $1;
        }
        if ( /^build type: +(.*)/ ) {
            $buildtype = $1;
        }
    }
}

print $index <<EOF
## DIVINE Functional Test Results

**summary**: $summary \\
**DIVINE**: $divine \\
**build type**: $buildtype \\
**build date**: $builddate

<table class="test_results">
EOF
;

if ( @flavours > 1 ) {
    print $index "<tr><td></td>";
    for my $f ( @flavours ) {
        print $index "<td>$f</td>";
    }
    print $index "</tr>\n";
}
for my $t ( @tests ) {
    print $index "<tr><td class=\"test\">$t</td>";
    for my $f ( @flavours ) {
        my $id = "$f:$t";
        my $report = "${f}_${t}.txt";
        $report =~ s|/|_|g;
        my $origreport = "$id.txt";
        $origreport =~ s|/|_|g;

        my $res = $results{ $id };
        $res = "none" unless $res;
        print $index "<td class=\"test_result test_$res\">[$res](./$report)</td>";
        system( "cp", catfile( $dir, $origreport ), catfile( $out, $report ) );
    }
    print $index "</tr>\n";
}

print $index "</table>\n";
close( $index );

system( "pandoc", "--to=html", "-s", "--smart", "--template", catfile( $webdir, "template.html" ), catfile( $out, "index.md" ), "-o", catfile( $out, "index.html" ) );
system( "cp", catfile( $webdir, "style.css" ), catfile( $out, "style.css" ) );
# system( "sed", "-i", 's/href="style.css"/href="..\/..\/style.css"/', catfile( $out, "index.html" ) );

$/ = undef;
open( my $ii, "<", catfile( $out, "index.html" ) );
my $indexcont = <$ii>;
close( $ii );

sub procLink {
    my ( $pre, $href, $post ) = @_;
    if ( ( $href =~ /^http/ ) || ( $href =~ m|^\./| ) || ( $href =~ m|^/| ) ) {
        $href =~ s|^\./|| if ( $href =~ m|^\./| );
        return "$pre$href$post";
    }
    else {
        return "$pre//$href$post";
    }
}

$indexcont =~ s/([<]a[^>]*href=["'])([^"']*)(['"])/procLink( $1, $2, $3 )/gems;
open( my $oi, ">", catfile( $out, "index.html" ) );
print $oi $indexcont;
close( $oi );

print STDERR "file://" . catfile( $out, "index.html" ) . "\n";
