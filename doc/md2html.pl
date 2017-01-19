use strict;

my %V;

for ( @ARGV )
{
    $V{$1} = $2 if ( m,(.*?)=(.*), );
}

$V{report} = `cat report.txt` if ( -e "report.txt" );

open(PD, "|pandoc --to=html -s --smart" .
         " --email-obfuscation=javascript" .
         " -o -" .
         " --template template.html" .
         " -V version:$V{version}" .
         " $V{opts}" );

for ( @ARGV )
{
    next if ( m,=, );
    local $/;
    open FILE, $_;
    while (<FILE>)
    {
        for my $k ( keys %V ) { s,\@${k}@,$V{$k},g; }
        s,DIVINE,:DIVINE:,g;
        print PD;
    }
}

close( PD );
