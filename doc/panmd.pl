use strict;

my %V;
my @O;
my @F;

for ( @ARGV )
{
    if ( m,^-, )
    {
        push @O, $_;
    }
    elsif ( m,(.*?)=(.*), )
    {
        $V{$1} = $2;
    }
    else
    {
        push @F, $_;
    }
}

$V{report} = `cat report.txt` if ( -e "report.txt" );
my $template;
$template = "--template template.html" if ( -f "template.html" );

open(PD, "|pandoc -o - -s --smart" .
         " --email-obfuscation=javascript" .
         " $template" .
         " -V version:$V{version}" .
         " $V{opts} @O" );

for ( @F )
{
    local $/;
    open FILE, $_;
    while (<FILE>)
    {
        for my $k ( keys %V ) { s,\@${k}@,$V{$k},g; }
        s,DIVINE,:DIVINE:,g if ( $template );
        print PD;
    }
    print PD "\n\n";
}

close( PD );
