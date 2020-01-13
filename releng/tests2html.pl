#!/usr/bin/env perl
use warnings;
use strict;

my %codemap = ( passed => "✓", failed => "✗", warnings => "!",
                skipped => "–", timeout => "T", started => "…" );
my ( $table, $lastcat ) = ( "", "" );
my %rescount;

while ( <> )
{
    m,^([^\s]*)\s+(.*)$,m;
    my ( $test, $status, $link, $schar ) = ( $1, $2, "$1.txt", $codemap{$2} );

    my $cat = ( split( m,/,, $test ) )[ 0 ];
    $cat =~ s,vanilla:,,;
    $link =~ s,/,_,g;
    ++ $rescount{$status};

    unless ( $cat eq $lastcat )
    {
        $table .= "</td></tr>\n" if ( $lastcat );
        $table .= qq(<tr><td class="test-name"><div>$cat</div></td>);
        $table .= qq(<td class="test test-compact"><div>);
    }
    $table .= qq(<a title="$test" class="test test-$status" href="./test/$link">$schar</a>&#8203;);
    $lastcat = $cat;
}

my @summary = map { qq(<span class="test-$_">$rescount{$_} $_</span>) } ( keys %rescount );
my $summary = join ", ", @summary;

print qq(<div class="test-compact"><table class="test test-compact">);
print $table;
print qq(</table></div>\n);
print qq(<p>summary: $summary</p>);
