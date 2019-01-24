#!/bin/perl -w

use strict;

#
# Get R Clock Timing
#
# Parse the specific I2C Exerciser timing csv that contains the 8D
# failure and output all the R clock timings.
#
# The idea for this utility started off as a full blown I2C bus state
# machine. To save time I decided not to write the branches of the
# machine that weren't actually taken by the specific file of
# interest. After simulating the state machine on paper to verify its
# accuracy I realised that the file is just one specific I2C
# transaction repeated over and over. Even faster is to just write a
# script that expects each line in its order and outputs the timing
# as it finds it. So this is big and ugly looking but was quick to
# code and get the data I needed.
#
# History
#
# v0.1  2010-Feb-08  Stuart MacDonald
#
# First draft.
#

#my $filename = "114d quick test 2010-02-18 17-32 - Timing.csv";
#my $rclockfilename = "114d quick test 2010-02-18 17-32 - Timing - R Clock.txt";
#my $fixedfilename = "114d quick test 2010-02-18 17-32 - Timing - Stu Fixed.csv";
#my $filename = "Buffer full - Timing.csv";
#my $rclockfilename = "Buffer full - Timing - R Clock.txt";
#my $fixedfilename = "Buffer full - Timing - Stu Fixed.csv";
my $filename = "8D 114d_0001 (2010-02-19 2) - Timing.csv";
my $rclockfilename = "8D 114d_0001 (2010-02-19 2) - Timing - R Clock.txt";
my $fixedfilename = "8D 114d_0001 (2010-02-19 2) - Timing - Stu Fixed.csv";
# my $num_lines = 20,939,156;

# This is how to get a static variable in Perl; use lexical scoping
BEGIN {
	my $r_clock_start;
	my $last_sda = 1;
	my $last_scl = 1;

	sub expect {
		my $index = shift;
		my $expect_sda = shift;
		my $expect_scl = shift;
		my $action = shift;
	
		my $new_sda;
		my $new_scl;
		my $new_ns;
	
NEXT_LINE:
		if (!defined($_ = <FILE>)) {
			if ($index == 1) {
				exit;
			} else {
				die "Error: Unexpected EOF, index '$index', '$filename' line '$.'\n";
			}
		}
		($new_sda, $new_scl, $new_ns) = /^(\d+),\s+(\d+),\s+(\d+)$/;
		if (($new_sda == 1 && $new_scl == 1) && ($last_sda == 1 && $last_scl == 1)) {
			print STDOUT "Warning: Ignoring spurious idle, index '$index', '$filename' line '$.'\n";
			goto NEXT_LINE;
		}
		#if (($new_sda == 0 && $new_scl == 0) && ($last_sda == 0 && $last_scl == 0)) {
		if ($new_sda == $last_sda && $new_scl == $last_scl) {
			# Corelis seems to have missed quite a few rising edges in this specific location of the sequence, autofix it...
			if ($index == 27) {
				print STDOUT "Warning: Missing rising edge added, '$filename' line '$.', ns '$new_ns'\n";
				# ...by pretending we saw the edge...
				$last_sda = 0;
				$last_scl = 1;
				# ...writing out the missing edge...
				print FIXEDFILE "0,\t1,\t0\n";
				# ..."unreading" the line we just read (seek back to the start of the line from the current file position)...
				seek(FILE, -length($_) - 1, 1);
				# ...resetting the current line number...
				$.--;
				# ...aaaand letting the expect parsing proceed as if the line had been actually read.
				return;
			# Missing falling edge here, autofix...
			} elsif ($index == 58) {
				print STDOUT "Warning: Missing falling edge added, '$filename' line '$.', ns '$new_ns'\n";
				# ...by pretending we saw the edge...
				$last_sda = 0;
				$last_scl = 0;
				# ...writing out the missing edge...
				print FIXEDFILE "0,\t0,\t0\n";
				# ..."unreading" the line we just read (seek back to the start of the line from the current file position)...
				seek(FILE, -length($_) - 1, 1);
				# ...resetting the current line number...
				$.--;
				# ...aaaand letting the expect parsing proceed as if the line had been actually read.
				return;
			} else {
				print STDOUT "Warning: Ignoring spurious quiet, index '$index', '$filename' line '$.'\n";
				goto NEXT_LINE;
			}
		}
		$last_sda = $new_sda;
		$last_scl = $new_scl;
		if ($new_sda != $expect_sda) {
			# TODO: Update here
			# This is the last good I2C bit transition on the bus; jump to end of main expect loop
			# if ($. == 19759608) {
				# goto BOTTOM;
			# }
			die "Error: Expected SDA '" . $expect_sda . "' read '" . $new_sda . "' instead, index '$index', '$filename' line '$.'\n";
		}
		if ($new_scl != $expect_scl) {
			die "Error: Expected SCL '" . $expect_scl . "' read '" . $new_scl . "' instead, index '$index', '$filename' line '$.'\n";
		}
		if ($action eq "r_clock_start") {
			$r_clock_start = $new_ns;
		} elsif ($action eq "r_clock_stop") {
			print RCLOCKFILE "\"" . ($new_ns - $r_clock_start) . "\",\"" . $r_clock_start . "\",\"" . $new_ns . "\"\n";
		}
		print FIXEDFILE;
	}
}

if (!open(FILE, $filename)) {
	die "Error: Couldn't open '$filename'\n";
}
if (!open(RCLOCKFILE, '>', $rclockfilename)) {
	die "Error: Couldn't open '$rclockfilename'\n";
}
if (!open(FIXEDFILE, '>', $fixedfilename)) {
	die "Error: Couldn't open '$fixedfilename'\n";
}

# Skip the header line
if (!defined($_ = <FILE>)) {
	die "Error: Unexpected EOF '$filename' line '$.'\n";
}
print FIXEDFILE;

# Output a header for the R clock file
print RCLOCKFILE "\"Delay\",\"Start (ns)\",\"End (ns)\"\n";

# Start the parsing
TOP:

# START
expect(1, 0, 1, "");
expect(2, 0, 0, "");
# addr 0x34, 0
expect(3, 0, 1, "");
expect(4, 0, 0, "");
# addr 0x34, 0
expect(5, 0, 1, "");
expect(6, 0, 0, "");
# addr 0x34, 1
expect(7, 1, 0, "");
expect(8, 1, 1, "");
expect(9, 1, 0, "");
# addr 0x34, 1
expect(10, 1, 1, "");
expect(11, 1, 0, "");
# addr 0x34, 0
expect(12, 0, 0, "");
expect(13, 0, 1, "");
expect(14, 0, 0, "");
# addr 0x34, 1
expect(15, 1, 0, "");
expect(16, 1, 1, "");
expect(17, 1, 0, "");
# addr 0x34, 0
expect(18, 0, 0, "");
expect(19, 0, 1, "");
expect(20, 0, 0, "");
# R
expect(21, 1, 0, "");
expect(22, 1, 1, "");
expect(23, 1, 0, "r_clock_start");
# ACK from slave
expect(24, 0, 0, "");
expect(25, 0, 1, "r_clock_stop");
expect(26, 0, 0, "");
# data 0x27, 0
expect(27, 0, 1, "");
expect(28, 0, 0, "");
# data 0x27, 0
expect(29, 0, 1, "");
expect(30, 0, 0, "");
# data 0x27, 1
expect(31, 1, 0, "");
expect(32, 1, 1, "");
expect(33, 1, 0, "");
# data 0x27, 0
expect(34, 0, 0, "");
expect(35, 0, 1, "");
expect(36, 0, 0, "");
# data 0x27, 0
expect(37, 0, 1, "");
expect(38, 0, 0, "");
# data 0x27, 1
expect(39, 1, 0, "");
expect(40, 1, 1, "");
expect(41, 1, 0, "");
# data 0x27, 1
expect(42, 1, 1, "");
expect(43, 1, 0, "");
# data 0x27, 1
expect(44, 1, 1, "");
expect(45, 1, 0, "");
# ACK from master
expect(46, 0, 0, "");
expect(47, 0, 1, "");
expect(48, 0, 0, "");
# data 0xc3, 1
expect(49, 1, 0, "");
expect(50, 1, 1, "");
expect(51, 1, 0, "");
# data 0xc3, 1
expect(52, 1, 1, "");
expect(53, 1, 0, "");
# data 0xc3, 0
expect(54, 0, 0, "");
expect(55, 0, 1, "");
expect(56, 0, 0, "");
# data 0xc3, 0
expect(57, 0, 1, "");
expect(58, 0, 0, "");
# data 0xc3, 0
expect(59, 0, 1, "");
expect(60, 0, 0, "");
# data 0xc3, 0
expect(61, 0, 1, "");
expect(62, 0, 0, "");
# data 0xc3, 1
expect(63, 1, 0, "");
expect(64, 1, 1, "");
expect(65, 1, 0, "");
# data 0xc3, 1
expect(66, 1, 1, "");
expect(67, 1, 0, "");
# NAK from master
expect(68, 1, 1, "");
expect(69, 1, 0, "");
# STOP
expect(70, 0, 0, "");
expect(71, 0, 1, "");
expect(72, 1, 1, "");

goto TOP;

# Finish copying the remainder of the timing file to the fixed file, otherwise even 'diff -H' crashes!
# diff -Hu crashes anyway. :-(
BOTTOM:

while (<FILE>) {
	print FIXEDFILE;
}
