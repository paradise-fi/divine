#! /usr/bin/perl
# NIPS Asm - New Implementation of Promela Semantics Assembler
# Copyright (C) 2005: Stefan Schuermans <stefan@schuermans.info>
#                     Michael Weber <michaelw@i2.informatik.rwth-aachen.de>
#                     Lehrstuhl fuer Informatik II, RWTH Aachen
# Copyleft: GNU public license - http://www.gnu.org/copyleft/gpl.html

use strict;

# add directory of script to include directory
my $incdir=$0;
$incdir =~ s/[^\/]*$//;
$incdir = "./" if( $incdir eq "" );
push @INC, $incdir;

# escape a string
sub escape_str
{
  my $str = shift;
  $str =~ s/\0/\\0/g;
  $str =~ s/\r/\\r/g;
  $str =~ s/\n/\\n/g;
  $str =~ s/\t/\\t/g;
  $str =~ s/"/\\"/g;
  $str =~ s/'/\\'/g;
  my $i;
  for( $i = 1; $i < 32; $i++ )
  {
    my $c = pack( "C", $i );
    my $h = sprintf( "%02X", $i );
    $str =~ s/$c/\\x$h/g;
  }
  return $str;
}

# convert a byte, a word, a dword to binary
sub byte2bin
{
  my $value = shift;
  return pack( "C", $value & 0xFF );
}
sub word2bin
{
  my $value = shift;
  return pack( "CC", ($value >> 8) & 0xFF,
                     $value & 0xFF );
}
sub dword2bin
{
  my $value = shift;
  return pack( "CCCC", ($value >> 24) & 0xFF,
                       ($value >> 16) & 0xFF,
                       ($value >> 8) & 0xFF,
                       $value & 0xFF );
}

# convert a byte, a word, a dword from binary
sub bin2byte
{
  my @data = unpack( "C", shift );
  return @data[0];
}
sub bin2word
{
  my @data = unpack( "CC", shift );
  return @data[0] << 8 | @data[1];
}
sub bin2dword
{
  my @data = unpack( "CCCC", shift );
  return @data[0] << 24 | @data[1] << 16 | @data[2] << 8 | @data[3];
}

# write a byte, a word, a dword to a binary file
sub wr_byte
{
  my $filehandle = shift;
  my $byte = shift;
  print $filehandle byte2bin( $byte );
}
sub wr_word
{
  my $filehandle = shift;
  my $word = shift;
  print $filehandle word2bin( $word );
}
sub wr_dword
{
  my $filehandle = shift;
  my $dword = shift;
  print $filehandle dword2bin( $dword );
}

# read a byte, a word, a dword from a binary file
sub rd_byte
{
  my $filehandle = shift;
  my $data;
  read $filehandle, $data, 1;
  return bin2byte( $data );
}
sub rd_word
{
  my $filehandle = shift;
  my $data;
  read $filehandle, $data, 2;
  return bin2word( $data );
}
sub rd_dword
{
  my $filehandle = shift;
  my $data;
  read $filehandle, $data, 4;
  return bin2dword( $data );
}

# write a string to a binary file
sub wr_string
{
  my $filehandle = shift;
  my $str = shift;
  wr_word( $filehandle, length( $str ) + 1 );
  print $filehandle $str . "\0";
}

# read a string from a binary file
sub rd_string
{
  my $filehandle = shift;
  my $str_sz = rd_word( $filehandle );
  my $str;
  read $filehandle, $str, $str_sz;
  $str =~ s/\0.*$//;
  return $str;
}

# write size to binary file
sub wr_size_tmp
{
  my $filehandle = shift;
  my $pos = tell $filehandle;
  wr_dword( $filehandle, 0 );
  return $pos;
}
sub wr_size_fillin
{
  my $filehandle = shift;
  my $sz_pos = shift;
  my $pos = tell $filehandle;
  seek $filehandle, $sz_pos, 0;
  wr_dword( $filehandle, $pos - $sz_pos - 4 );
  seek $filehandle, $pos, 0;
}

1;
sub get_instructions
{
  my ($cflow, $error) = (1, 1);
  my @instructions = (
    [ 0x00, [ "NOP" ] ],
    [ 0x01, [ "LDC", "!const4" ] ],
    [ 0x02, [ "LDV", "L", "1u" ] ],
    [ 0x03, [ "LDV", "L", "1s" ] ],
    [ 0x04, [ "LDV", "L", "2u" ] ],
    [ 0x05, [ "LDV", "L", "2s" ] ],
    [ 0x06, [ "LDV", "L", "4" ] ],
    [ 0x07, [ "LDV", "G", "1u" ] ],
    [ 0x08, [ "LDV", "G", "1s" ] ],
    [ 0x09, [ "LDV", "G", "2u" ] ],
    [ 0x0A, [ "LDV", "G", "2s" ] ],
    [ 0x0B, [ "LDV", "G", "4" ] ],
    [ 0x0C, [ "STV", "L", "1u" ] ],
    [ 0x0D, [ "STV", "L", "1s" ] ],
    [ 0x0E, [ "STV", "L", "2u" ] ],
    [ 0x0F, [ "STV", "L", "2s" ] ],
    [ 0x10, [ "STV", "L", "4" ] ],
    [ 0x11, [ "STV", "G", "1u" ] ],
    [ 0x12, [ "STV", "G", "1s" ] ],
    [ 0x13, [ "STV", "G", "2u" ] ],
    [ 0x14, [ "STV", "G", "2s" ] ],
    [ 0x15, [ "STV", "G", "4" ] ],
    [ 0x16, [ "TRUNC", "!const1" ] ],
    [ 0x18, [ "LDS", "timeout" ] ],
    [ 0x19, [ "LDS", "pid" ] ],
    [ 0x1A, [ "LDS", "nrpr" ] ],
    [ 0x1B, [ "LDS", "last" ] ],
    [ 0x1C, [ "LDS", "np" ] ],
    [ 0x20, [ "ADD" ] ],
    [ 0x21, [ "SUB" ] ],
    [ 0x22, [ "MUL" ] ],
    [ 0x23, [ "DIV" ] ],
    [ 0x24, [ "MOD" ] ],
    [ 0x25, [ "NEG" ] ],
    [ 0x26, [ "NOT" ] ],
    [ 0x27, [ "AND" ] ],
    [ 0x28, [ "OR" ] ],
    [ 0x29, [ "XOR" ] ],
    [ 0x2A, [ "SHL" ] ],
    [ 0x2B, [ "SHR" ] ],
    [ 0x2C, [ "EQ" ] ],
    [ 0x2D, [ "NEQ" ] ],
    [ 0x2E, [ "LT" ] ],
    [ 0x2F, [ "LTE" ] ],
    [ 0x30, [ "GT" ] ],
    [ 0x31, [ "GTE" ] ],
    [ 0x32, [ "BNOT" ] ],
    [ 0x33, [ "BAND" ] ],
    [ 0x34, [ "BOR" ] ],
    [ 0x40, [ "ICHK", "!const1" ], $error ],
    [ 0x41, [ "BCHK" ], $error ],
    [ 0x48, [ "JMP", "!addr" ], $cflow ],
    [ 0x49, [ "JMPZ", "!addr" ], $cflow ],
    [ 0x4A, [ "JMPNZ", "!addr" ], $cflow ],
    [ 0x4B, [ "LJMP", "!address" ], $cflow ],
    [ 0x50, [ "TOP", "!reg" ] ],
    [ 0x51, [ "POP", "!reg" ] ],
    [ 0x52, [ "PUSH", "!reg" ] ],
    [ 0x53, [ "POPX" ] ],
    [ 0x54, [ "INC", "!reg" ] ],
    [ 0x55, [ "DEC", "!reg" ] ],
    [ 0x56, [ "LOOP", "!reg", "!addr" ], $cflow ],
    [ 0x58, [ "CALL", "!addr" ], $cflow ],
    [ 0x59, [ "RET" ], $cflow ],
    [ 0x5A, [ "LCALL", "!address" ], $cflow ],
    [ 0x60, [ "CHNEW", "!const1", "!const1" ] ],
    [ 0x61, [ "CHMAX" ] ],
    [ 0x62, [ "CHLEN" ] ],
    [ 0x63, [ "CHFREE" ] ],
    [ 0x64, [ "CHADD" ] ],
    [ 0x65, [ "CHSET" ] ],
    [ 0x66, [ "CHGET" ] ],
    [ 0x67, [ "CHDEL" ] ],
    [ 0x68, [ "CHSORT" ] ],
    [ 0x6B, [ "CHROT" ] ],
    [ 0x6C, [ "CHSETO", "!const1" ] ],
    [ 0x6D, [ "CHGETO", "!const1" ] ],
    [ 0x70, [ "NDET", "!addr" ] ],
    [ 0x72, [ "ELSE", "!addr" ] ],
    [ 0x73, [ "UNLESS", "!addr" ] ],
    [ 0x74, [ "NEX" ], $cflow ],
    [ 0x75, [ "NEXZ" ], $cflow ],
    [ 0x76, [ "NEXNZ" ], $cflow ],
    [ 0x78, [ "STEP", "N", "!const1" ], $cflow ],
    [ 0x79, [ "STEP", "A", "!const1" ], $cflow ],
    [ 0x7A, [ "STEP", "I", "!const1" ], $cflow ],
    [ 0x7B, [ "STEP", "T", "!const1" ], $cflow ],
    [ 0x80, [ "RUN", "!const1", "!const1", "!addr" ] ],
    [ 0x81, [ "LRUN", "!const1", "!const1", "!address" ] ],
    [ 0x84, [ "GLOBSZ", "!const1" ] ],
    [ 0x85, [ "LOCSZ", "!const1" ] ],
    [ 0x86, [ "GLOBSZX", "!const2" ] ],
    [ 0x88, [ "FCLR" ] ],
    [ 0x89, [ "FGET", "!const1" ] ],
    [ 0x8A, [ "FSET", "!const1" ] ],
    [ 0x8C, [ "BGET", "!reg", "!const1" ] ],
    [ 0x8D, [ "BSET", "!reg", "!const1" ] ],
    [ 0x90, [ "PRINTS", "!const2" ] ],
    [ 0x91, [ "PRINTV", "!const1" ] ],
    [ 0x92, [ "LDVA", "L", "1u", "!const1" ] ],
    [ 0x93, [ "LDVA", "L", "1s", "!const1" ] ],
    [ 0x94, [ "LDVA", "L", "2u", "!const1" ] ],
    [ 0x95, [ "LDVA", "L", "2s", "!const1" ] ],
    [ 0x96, [ "LDVA", "L", "4", "!const1" ] ],
    [ 0x97, [ "LDVA", "G", "1u", "!const1" ] ],
    [ 0x98, [ "LDVA", "G", "1s", "!const1" ] ],
    [ 0x99, [ "LDVA", "G", "2u", "!const1" ] ],
    [ 0x9A, [ "LDVA", "G", "2s", "!const1" ] ],
    [ 0x9B, [ "LDVA", "G", "4", "!const1" ] ],
    [ 0x9C, [ "STVA", "L", "1u", "!const1" ] ],
    [ 0x9D, [ "STVA", "L", "1s", "!const1" ] ],
    [ 0x9E, [ "STVA", "L", "2u", "!const1" ] ],
    [ 0x9F, [ "STVA", "L", "2s", "!const1" ] ],
    [ 0xA0, [ "STVA", "L", "4", "!const1" ] ],
    [ 0xA1, [ "STVA", "G", "1u", "!const1" ] ],
    [ 0xA2, [ "STVA", "G", "1s", "!const1" ] ],
    [ 0xA3, [ "STVA", "G", "2u", "!const1" ] ],
    [ 0xA4, [ "STVA", "G", "2s", "!const1" ] ],
    [ 0xA5, [ "STVA", "G", "4", "!const1" ] ],
    [ 0xB0, [ "LDA", "!address" ] ],
    [ 0xB4, [ "PCVAL" ] ],
    [ 0xB8, [ "LVAR", "1u" ] ],
    [ 0xB9, [ "LVAR", "1s" ] ],
    [ 0xBA, [ "LVAR", "2u" ] ],
    [ 0xBB, [ "LVAR", "2s" ] ],
    [ 0xBC, [ "LVAR", "4" ] ],
    [ 0xBE, [ "ENAB" ] ],
    [ 0xC0, [ "MONITOR" ] ],
    [ 0xC4, [ "KILL" ] ],
    [ 0xD0, [ "LDB", "L" ] ],
    [ 0xD1, [ "LDB", "G" ] ],
    [ 0xD2, [ "STB", "L" ] ],
    [ 0xD3, [ "STB", "G" ] ],
    [ 0xD4, [ "LDV", "L", "2u", "LE" ] ],
    [ 0xD5, [ "LDV", "L", "2s", "LE" ] ],
    [ 0xD6, [ "LDV", "L", "4", "LE" ] ],
    [ 0xD7, [ "LDV", "G", "2u", "LE" ] ],
    [ 0xD8, [ "LDV", "G", "2s", "LE" ] ],
    [ 0xD9, [ "LDV", "G", "4", "LE" ] ],
    [ 0xDA, [ "STV", "L", "2u", "LE" ] ],
    [ 0xDB, [ "STV", "L", "2s", "LE" ] ],
    [ 0xDC, [ "STV", "L", "4", "LE" ] ],
    [ 0xDD, [ "STV", "G", "2u", "LE" ] ],
    [ 0xDE, [ "STV", "G", "2s", "LE" ] ],
    [ 0xDF, [ "STV", "G", "4", "LE" ] ],
  );
  return @instructions;
}

sub instruction_cfun {
    my ($op, $params) = @_;

    my $name = "instr";
    for (@{$params}) {
	next if /^!/;
	if (/^[0-9]/) {
	    $name .= lc($_);
	    next;
	}
	$name .= "_".lc($_);
    }
    return $name;
}

my @instructions = get_instructions( );

print <<EOF;

NIPS Asm - New Implementation of Promela Semantics Assembler
Copyright (C) 2005: Stefan Schuermans <stefan\@schuermans.info>
                    Michael Weber <michaelw\@i2.informatik.rwth-aachen.de>
                    Lehrstuhl fuer Informatik II, RWTH Aachen
Copyleft: GNU public license - http://www.gnu.org/copyleft/gpl.html

EOF

# parse parameters

die "usage: divine-mc.compile-pml input.pml [<output.b> [<output.l>]]" if( @ARGV < 1 || @ARGV > 3 );
my ( $file_pml, $file_asm, $file_byte, $file_list ) = ( @ARGV[0], @ARGV[0], @ARGV[0], @ARGV[0] );
$file_asm =~ s/$/.s/;
$file_byte =~ s/(\.pml)?$/.b/;
$file_byte = @ARGV[1] if( @ARGV >= 2 );
$file_list =~ s/(\.pml)?$/.l/;
$file_list = @ARGV[2] if( @ARGV >= 3 );

# parse input

my $jarfiles = unpack_jars();

print "compiling file \"$file_pml\"...\n";

((system "java -cp \"$jarfiles\" xml.XmlSerializer $file_pml") == 0) or die "Fatal: error parsing $file_pml";
((system "java -cp \"$jarfiles\" CodeGen.CodeGen $file_pml.ast.xml") == 0) or die "Fatal: error compiling $file_pml";

print "assembling file \"$file_asm\"...\n";

open ASM, "<".$file_asm or die "could not open input file \"$file_asm\"";

my ( $line_no, $line, $module );
my $modflags = 0;
my $addr = 0;
my @bytecodes = ( );
my @flags = ( );
my @strings = ( );
my @srclocs = ( );
my @strinfs = ( );
$module = "";
my @modules = ( );
for( $line_no = 1; $line = <ASM>; $line_no++ )
{
  
  # remove newline, whitespace, comment

  chomp $line;
  chomp $line;
  $line =~ s/\t/ /g;
  $line =~ s/^ *([^;]*)(;.*)?$/\1/;

  # get label and split words

  $line =~ /^(([A-Za-z][A-Za-z0-9_]*): *)?(.*)$/;
  my $label = $2;
  my @words = split /[ ,]+/, $3;

  # get string possibly contained in line

  my $str = undef;
  $str = eval( $1 ) if( $line =~ /^[^"]*("([^"]|\\.)*").*$/ );

  # empty line

  if( @words <= 0 || (@words == 1 && @words[0] eq "") )
  {
    # save empty bytecode for this line if there is a label
    push @bytecodes, [ $addr, $label, [ ], [ ] ] if( $label ne "" );
    next;
  }

  # start of new module

  if( @words[0] eq "!module" )
  {
    die "\"!module\" needs a string in line $line_no" if( $str eq undef );
    push @modules, [ $module, $modflags, [ @bytecodes ], [ @strings ] ] if( @bytecodes > 0 or @strings > 0 );
    $module = $str;
    $modflags = 0;
    $addr = 0;
    @bytecodes = ( );
    @flags = ( );
    @strings = ( );
    @srclocs = ( );
    @strinfs = ( );
    next;
  }

  # module flags

  if( @words[0] eq "!modflags" )
  {
    my $flag;
    foreach $flag (@words)
    {
      if( $flag eq "monitor" ) { $modflags |= 0x00000001; }
    }
    next;
  }

  # flags for address

  if( @words[0] eq "!flags" || @words[0] eq "!flags_addr" )
  {
    my $ad = $addr;
    my $i = 1;
    if( @words[0] eq "!flags_addr" )
    {
      if( @words[1] =~ /^0*[xX]/ )
        { $ad = hex( @words[1] ); }
      else
        { $ad = int( @words[1] ); }
      $i = 2;
    }
    my $fl = 0;
    for( ; $i < @words; $i++ )
    {
      if( @words[$i] eq "progress" )
        { $fl |= 0x00000001; }
      elsif( @words[$i] eq "accept" )
        { $fl |= 0x00000002; }
      else
        { die "unknown flag \"@words[$i]\" in line $line_no"; }
    }
    if( @flags > 0 && @{@flags[@flags-1]}[0] > $ad ) {
	die "flags out of order in line $line_no"
    } elsif (@flags > 0 && @{@flags[@flags-1]}[0] == $ad) {
	# add more flags
	@{@flags[@flags-1]}[1] |= $fl;
    } else {
        push @flags, [ $ad, $fl ] if( $fl != 0 );
    }	
    next;
  }

  # string to put into string table

  if( @words[0] eq "!string" )
  {
    die "\"!string\" needs a number in line $line_no" if( @words[1] !~ /^[0-9]+$/ );
    die "\"!string\" needs a string in line $line_no" if( $str eq undef );
    my $i = int( @words[1] );
    die "duplicate definition of string $i in line $line_no" if( $i < @strings and @strings[$i] ne undef );
    my $j;
    for( $j = @strings; $j < $i; $j++ )
      { @strings[$j] = undef; }
    @strings[$i] = $str;
    next;
  }  

  # source location

  if( @words[0] eq "!srcloc" || @words[0] eq "!srcloc_addr" )
  {
    my $ad = $addr;
    my $i = 1;
    if( @words[0] eq "!srcloc_addr" )
    {
      if( @words[1] =~ /^0*[xX]/ )
        { $ad = hex( @words[1] ); }
      else
        { $ad = int( @words[1] ); }
      $i = 2;
    }
    my $line = int( @words[$i] );
    my $col = int( @words[$i+1] );
    die "source location out of order in line $line_no" if( @srclocs > 0 && @{@srclocs[@srclocs-1]}[0] > $ad );
    push @srclocs, [ $ad, $line, $col ];
    next;
  }

  # structure information

  if( @words[0] eq "!strinf" || @words[0] eq "!strinf_addr" )
  {
    my $ad = $addr;
    my $i = 1;
    if( @words[0] eq "!strinf_addr" )
    {
      if( @words[1] =~ /^0*[xX]/ )
        { $ad = hex( @words[1] ); }
      else
        { $ad = int( @words[1] ); }
      $i = 2;
    }
    my $code = 0xFF;
    $code = 0x00 if( @words[$i] eq "begin" );
    $code = 0x01 if( @words[$i] eq "end" );
    $code = 0x02 if( @words[$i] eq "middle" );
    my $type = @words[$i+1] . "";
    die "invalid type \"$type\" in structure information in line $line_no" if( $type !~ /^[A-Za-z0-9_]+$/ );
    my $name = @words[$i+2] . "";
    die "invalid name \"$name\" in structure information in line $line_no" if( $name !~ /^[A-Za-z0-9_.]*$/ );
    die "structure information out of order in line $line_no" if( @strinfs > 0 && @{@strinfs[@strinfs-1]}[0] > $ad );
    push @strinfs, [ $ad, $code, $type, $name ];
    next;
  }

  # find instruction in table

  my $ok = 0;
  my ( $opcode, $params );
  for (@instructions)
  {
    ( $opcode, $params ) = @{$_};
    if( @words == @{$params} )
    {
      my $i;
      for( $i = 0; $i < @{$params}; $i++ )
      {
        my $param = @{$params}[$i];
        my $word = @words[$i];
        last if( $param !~ /^\!/ and $param ne $word );
      }
      if( $i >= @{$params} )
      {
        $ok = 1;
        last;
      }
    }
  }
  die "invalid instruction \"@words\" in line $line_no" if( ! $ok );

  # process parameters and generate bytecode

  my @bytecode = ( $opcode );
  my $i;
  for( $i = 0; $i < @{$params}; $i++ )
  {

    # byte constant

    if( @{$params}[$i] eq "!const1" )
    {
      die "invalid constant \"".@words[$i]."\" in line $line_no" if( @words[$i] !~ /^-?[0-9]+$/ );
      my $val = int( @words[$i] );
      die "1-byte constant \"".@words[$i]."\" in line $line_no is out of range" if( $val > 0xFF or $val < -0x80 );
      push @bytecode, $val & 0xFF;
    }

    # word constant

    elsif( @{$params}[$i] eq "!const2" )
    {
      die "invalid constant \"".@words[$i]."\" in line $line_no" if( @words[$i] !~ /^-?[0-9]+$/ );
      my $val = int( @words[$i] );
      die "2-byte constant \"".@words[$i]."\" in line $line_no is out of range" if( $val > 0xFFFF or $val < -0x8000 );
      push @bytecode, ($val >> 8) & 0xFF;
      push @bytecode, $val & 0xFF;
    }

    # double-word constant

    elsif( @{$params}[$i] eq "!const4" )
    {
      die "invalid constant \"".@words[$i]."\" in line $line_no" if( @words[$i] !~ /^-?[0-9]+$/ );
      my $val = int( @words[$i] );
      push @bytecode, ($val >> 24) & 0xFF;
      push @bytecode, ($val >> 16) & 0xFF;
      push @bytecode, ($val >> 8) & 0xFF;
      push @bytecode, $val & 0xFF;
    }

    # register

    elsif( @{$params}[$i] eq "!reg" )
    {
      die "invalid register \"".@words[$i]."\" in line $line_no" if( @words[$i] !~ /^r([0-7])$/ );
      push @bytecode, int( $1 );
    }

    # relative address given by label

    elsif( @{$params}[$i] eq "!addr" )
    {
      die "invalid label \"".@words[$i]."\" in line $line_no" if( @words[$i] !~ /^[A-Za-z][A-Za-z0-9_]*$/ );
      push @bytecode, "addr1:".@words[$i]; # relative address of label takes 2 bytes
      push @bytecode, "addr0:".@words[$i];
    }

    # absolute address given by label

    elsif( @{$params}[$i] eq "!address" )
    {
      die "invalid label \"".@words[$i]."\" in line $line_no" if( @words[$i] !~ /^[A-Za-z][A-Za-z0-9_]*$/ );
      push @bytecode, "address3:".@words[$i]; # absolute address of label takes 4 bytes
      push @bytecode, "address2:".@words[$i];
      push @bytecode, "address1:".@words[$i];
      push @bytecode, "address0:".@words[$i];
    }

    # other parmeter type

    elsif( @{$params}[$i] =~ /^\!/ )
    {
      die "internal error: unknown parmeter type \"".@{$params}[$i]."\"\n";
    }
  }

  # save bytecode of this line

  push @bytecodes, [ $addr, $label, [ @bytecode ], [ @words ] ];
  $addr += @bytecode
}
push @modules, [ $module, $modflags, [ @bytecodes ], [ @flags ], [ @strings ], [ @srclocs], [ @strinfs ] ] if( @bytecodes > 0 or @strings > 0 );
die "no code found" if( @modules <= 0 );

# convert labels into addresses

print "converting labels to addresses...\n";

for (@modules)
{
  my ($module, $modflags, $bytecodes, $flags, $strings, $srclocs, $strinfs) = @{$_};

  for (@{$bytecodes})
  {
    my ( $addr, $label, $bc, $w ) = @{$_};
    for (@{$bc})
    {

      # byte in bytecode is a label

      if( $_ =~ /^addr(ess)?([0123]):([A-Za-z][A-Za-z0-9_]*)$/ )
      {
        my $rel = $1 eq "";
        my $byte = $2;
        my $lab = $3;

        # find declaration of this label

        my $ok = 0;
        my $ad = "";
        for (@{$bytecodes})
        {
          my ( $addr, $label, $bc, $w ) = @{$_};
          if( $label eq $lab )
          {
            $ad = $addr;
            $ok = 1;
            last;
          }
        }
        die "label \"$lab\" is not declared in module \"$module\"" if( ! $ok );

        # convert address into relative one

        if( $rel )
        {
          $ad -= $addr + @{$bc};
          die "destination label \"".$lab."\" in module \"$module\" is out of range" if( $ad > 0x7FFF or $ad < -0x8000 );
        }

        # put right byte address into bytecode

        $_ = ($ad >> ($byte * 8)) & 0xFF;
      }
    }

    # update this line

    $_ = [ $addr, $label, $bc, $w ];
  }
}

# output bytecode and listing

print "writing output files \"$file_byte\" and \"$file_list\"...\n";

open BYTE, ">".$file_byte or die "could not open bytecode output file \"$file_byte\"";
binmode BYTE;
open LIST, ">".$file_list or die "could not open list output file \"$file_list\"";

# file header

print BYTE "NIPS v2 ";
wr_word( \*BYTE, @modules + 0 );
print LIST "# this code was assembled according to \"NIPS v2 \"\n\n";

# modules

for (@modules)
{
  my ($module, $modflags, $bytecodes, $flags, $strings, $srclocs, $strinfs) = @{$_};

  # module header

  # sec_type
  print BYTE "mod ";
  # sec_sz (0 for now)
  my $sec_sz_pos = wr_size_tmp( \*BYTE );

  print LIST "!module \"" . escape_str( $module ) . "\"\n\n";

  # module_name
  wr_string( \*BYTE, $module );

  # part_cnt
  wr_word( \*BYTE, 6 );

  # module flags

  print BYTE "modf";
  wr_dword( \*BYTE, 4 );
  wr_dword( \*BYTE, $modflags );

  print LIST "!modflags";
  print LIST " monitor" if( $modflags & 0x00000001 );
  print LIST "\n\n";

  # code

  # part_type
  print BYTE "bc  ";
  # part_sz (0 for now)
  my $part_sz_pos = wr_size_tmp( \*BYTE );

  for (@{$bytecodes})
  {
    my ( $addr, $label, $bc, $w ) = @{$_};

    # byte code

    wr_byte( \*BYTE, $_ ) for (@{$bc});

    # hex dump of bytecode

    printf LIST "0x%08X:", $addr;
    printf LIST " 0x%02X", $_ for (@{$bc});

    # indentation

    my $i;
    for( $i = 0; $i < 48 - 7 - 5 * @{$bc}; $i++ ) { print LIST " "; }

    # source code

    print LIST " #";
    print LIST " $label:" if( $label ne "" );
    print LIST " $_" for (@{$w});
    print LIST "\n";
  }
  print LIST "\n";

  # part_sz
  wr_size_fillin( \*BYTE, $part_sz_pos );

  # flag table

  # part_type
  print BYTE "flag";
  # part_sz
  $part_sz_pos = wr_size_tmp( \*BYTE );

  # flag_cnt
  wr_word( \*BYTE, @{$flags} + 0 );

  my $i;
  for( $i = 0; $i < @{$flags}; $i++ )
  {
    my ($addr, $fl) = @{@{$flags}[$i]};
    # flag
    wr_dword( \*BYTE, $addr );
    wr_dword( \*BYTE, $fl );

    printf LIST "!flags_addr 0x%08X", $addr;
    print LIST " progress" if( $fl & 0x00000001 );
    print LIST " accept" if( $fl & 0x00000002 );
    print LIST "\n";
  }
  print LIST "\n";

  # part_sz
  wr_size_fillin( \*BYTE, $part_sz_pos );

  # string table

  # part_type
  print BYTE "str ";
  # part_sz
  $part_sz_pos = wr_size_tmp( \*BYTE );

  # str_cnt
  wr_word( \*BYTE, @{$strings} + 0 );

  for( $i = 0; $i < @{$strings}; $i++ )
  {
    my $str = @{$strings}[$i];
    if( $str ne undef )
    {
      wr_string( \*BYTE, $str );
      print LIST "!string $i \"" . escape_str( $str ) . "\"\n";
    }
    else
    {
      # empty string
      wr_string( \*BYTE, "" );
    }
  }
  print LIST "\n";

  # part_sz
  wr_size_fillin( \*BYTE, $part_sz_pos );

  # source location table

  # part_type
  print BYTE "sloc";
  # part_sz
  $part_sz_pos = wr_size_tmp( \*BYTE );

  # sloc_cnt
  wr_word( \*BYTE, @{$srclocs} + 0 );

  for( $i = 0; $i < @{$srclocs}; $i++ )
  {
    my ($addr, $line, $col) = @{@{$srclocs}[$i]};
    # srcloc
    wr_dword( \*BYTE, $addr );
    wr_dword( \*BYTE, $line );
    wr_dword( \*BYTE, $col );

    printf LIST "!srcloc_addr 0x%08X %d %d\n", $addr, $line, $col;
  }
  print LIST "\n";

  # part_sz
  wr_size_fillin( \*BYTE, $part_sz_pos );

  # structure information table

  # part_type
  print BYTE "stin";
  # part_sz
  $part_sz_pos = wr_size_tmp( \*BYTE );

  # stin_cnt
  wr_word( \*BYTE, @{$strinfs} + 0 );

  for( $i = 0; $i < @{$strinfs}; $i++ )
  {
    my ($addr, $code, $type, $name) = @{@{$strinfs}[$i]};
    # strinf
    wr_dword( \*BYTE, $addr );
    wr_byte( \*BYTE, $code );
    wr_string( \*BYTE, $type );
    wr_string( \*BYTE, $name );

    printf LIST "!strinf_addr 0x%08X", $addr;
    if( $code == 0x00 )
      { print LIST " begin "; }
    elsif( $code == 0x01 )
      { print LIST " end "; }
    elsif( $code == 0x02 )
      { print LIST " middle "; }
    else
      { print LIST " unknown "; }
    print LIST $type;
    print LIST " $name" if( $name ne "" );
    print LIST "\n";
  }
  print LIST "\n";

  # part_sz
  wr_size_fillin( \*BYTE, $part_sz_pos );

  # end of section

  # sec_sz
  wr_size_fillin( \*BYTE, $sec_sz_pos );
}
print "done...\n";
print "NOTE: Now, you can run divine-mc with \"$file_byte\" as input file\n";
