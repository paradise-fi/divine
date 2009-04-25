my $path = $ARGV[0];
shift @ARGV;

my $head = `cat "$ARGV[0]"`;
shift @ARGV;

my $bundle = 0;
$bundle = 1 if ( $path eq "BUNDLE" );
$path = "/tmp" if ( $bundle );

print $head;
print "sub unpack_jars() {\n";
print "    my \$out;";
for (@ARGV) {
    my $jar = $_;

    if ( $bundle ) {
        open JAR, "$jar";
        $in .= $_ while (<JAR>);
    }

    $jar =~ s,.*/(.*?),$1,;
    push @jars, "$path/$jar";

    if ( $bundle ) {
        $packed = pack("u", $in);
        print <<_EOP;
        open JAR, ">/tmp/$jar" or die "Failed to write '/tmp/$jar'";
        \$out = <<'_EOF';
${packed}_EOF
        print JAR unpack("u", \$out);
        close JAR;
_EOP
    }

    close JAR if ( $bundle );
}

print "    return join ':', (";
print "\"$_\", " for (@jars);
print ");\n}\n";
