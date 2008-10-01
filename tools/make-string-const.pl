my $head = $ARGV[0];
my $in = `cat "$ARGV[1]"`;

print $head;
for (split /\n/, $in) {
    $_ =~ s,(\\|"),\\$1,g;
    print '"' . $_ . "\\n\"\n";
}

print ";\n";
