#!/usr/bin/perl

my $author;
while (<>)
{
    if    ( /^patch/ ) {}
    elsif ( /^Author: (.*)/ ) { $author = $1; }
    elsif ( /^Date:   (.*)/ ) { print "  * $author, $1  \n"; }
    elsif ( /^    ([RMA]!?) \.\/(.*)/ ) { print "        $1 $2\n"; }
    elsif ( /^     \.\/(.*) -> (.*)/ ) { print "        $1 -> $2\n"; }
    else  { s/^  \* /  /; print "  $_"; }
}
