#!/usr/bin/perl

use strict;
use warnings;
use English;

system('./8srv&');
while( ! -e "ptsname.txt" ) {};
open(FH, '<ptsname.txt') or die $@;
my $line;
while(<FH>) {
    if( /dev\/pts/ ){
        $line = $_;
    }
}

my $exit_val = system("./8con --pty $line " . join(" ", @ARGV));
$exit_val = $exit_val >> 8;
#my $exit_val = ${^CHILD_ERROR_NATIVE};
system("rm ptsname.txt");

exit($exit_val);
