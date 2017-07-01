use JSON::PP;
use strict;
local $/;

open JSON, "pandoc-citeproc -j $ARGV[0] |";
my $bib = JSON::PP->new->decode(<JSON>);
my %sorted;

for ( @$bib )
{
    my $authors = join ", ", map { "$_->{given} $_->{family}" } @{$_->{author}};
    my $title = $_->{title};
    my $year = $_->{issued}->{"date-parts"}->[0]->[0];
    my $publisher = $_->{publisher};
    my $coll = $_->{"collection-title"};
    $coll = "volume $_->{volume} of $coll" if ( $coll && $_->{volume} );
    my $book = $_->{"container-title"};
    my $url = $_->{URL};
    my $note = $_->{note};

    $title = "[$title]($url)" if ( $url );
    $publisher = "$publisher, " if ( $publisher );
    $coll = ", $coll" if ( $coll );
    $book = "In *$book*, " if ( $book );
    $note = " $note" if ( $note );

    $sorted{$year} .= "$authors: **$title**. $book$publisher$year$coll.$note\n\n";
}

my $biburl = $ARGV[0];
$biburl =~ s,^(.*/)*,,;
print "This list of papers is also available as a  [bibtex file]($biburl).\n\n";

for ( reverse sort (keys %sorted) )
{
    print "# $_\n\n";
    print "$_" for ( $sorted{$_} );
    print "\n";
}
