#!/usr/bin/perl

use strict;
use warnings;
use English;

system('./8srv&');
while( ! -e "ptyname.txt" ) {};
open(FH, '<ptyname.txt') or die $@;
my $line;
while(<FH>) {
    if( /dev\/pts/ ){
        $line = $_;
    }
}

sleep(2);

my $exit_val = system("./8con --pty $line " . join(" ", @ARGV));
$exit_val = $exit_val >> 8;
#my $exit_val = ${^CHILD_ERROR_NATIVE};
system("rm ptyname.txt");

exit($exit_val);
