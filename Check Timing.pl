#!/bin/perl -w

use strict;

#
# Check Timing
#
# Parse an I2C Exerciser timing csv and output a file with relative
# ns timings and with any negative timings highlighted.
#
# History
#
# v0.1  2010-Feb-23  Stuart MacDonald
#
# First draft.
#

#my $filename = "Buffer full II - Timing.csv";
#my $relativefilename = "Buffer full II - Timing - Relative.txt";
my $filename = "8D 114d_0001 (2010-02-24) - Timing.csv";
my $relativefilename = "8D 114d_0001 (2010-02-24) - Timing - Relative.txt";

my $new_ns;
my $last_ns;

if (!open(FILE, $filename)) {
	die "Error: Couldn't open '$filename'\n";
}
if (!open(RELATIVEFILE, '>', $relativefilename)) {
	die "Error: Couldn't open '$relativefilename'\n";
}

# Pass the header line through
if (!defined($_ = <FILE>)) {
	die "Error: Unexpected EOF '$filename' line '$.'\n";
}
print RELATIVEFILE;

# Pass the first line through, no calculation yet
if (!defined($_ = <FILE>)) {
	die "Error: Unexpected EOF '$filename' line '$.'\n";
}
print RELATIVEFILE;
($last_ns) = /^[01],\t[01],\t(\d+)$/;

while (<FILE>) {
	($new_ns) = /^[01],\t[01],\t(\d+)$/;
	chomp;
	print RELATIVEFILE $_ . ",\t" . ($new_ns - $last_ns) . "\n";
	$last_ns = $new_ns;
}
