my $head = `cat "$ARGV[0]"`;
shift @ARGV;

print $head;
print "sub unpack_jars() {\n";
print "    my \$out;";
for (@ARGV) {
    my $jar = $_;
    open JAR, "$jar";
    $in .= $_ while (<JAR>);

    $jar =~ s,.*/(.*?),$1,;
    push @jars, "/tmp/$jar";
    $packed = pack("u", $in);
    print <<_EOP;
    open JAR, ">/tmp/$jar" or die "Failed to write '/tmp/$jar'";
    \$out = <<'_EOF';
${packed}_EOF
    print JAR unpack("u", \$out);
    close JAR;

_EOP
    close JAR;
}

print "    return join ':', (";
print "\"$_\", " for (@jars);
print ");\n}\n";
