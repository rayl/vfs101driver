#!/usr/bin/perl

# Convert parsed UsbSnoop logfile into vfs101 API calls
# 
# Copyright (c) 2010 Ray Lehtiniemi <rayl@mail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

use 5.6.0;
use strict;
use warnings;


#-----------------------------------------------------------------------------------
# Input file handling
#-----------------------------------------------------------------------------------

# All lines from the preprocessed tracefile. The current line being processed
# sits on the top of the array and is popped off when processing is complete.
my $curr = 0;
my @line;
chomp(@line = <>);

# return the current line
sub current_line {
	$line[$curr]
}

# go to next line, returning the current one
sub next_line {
	$line[$curr++]
}

# go back to the start of the input file
sub first_line {
	$curr = 0
}

# are there any lines left to process?
sub more_lines {
	defined current_line
}

# does the current line contain this pattern?
sub looking_at {
	more_lines and current_line =~ m/$_[0]/
}

# skip over all lines matching a pattern
sub drop {
	next_line while looking_at $_[0];
}



#-----------------------------------------------------------------------------------
# Input file handling
#-----------------------------------------------------------------------------------

# a table of command names, indexed by command id
my @cmd = (undef, qw(
     Reset
     GetVersion
     GetPrint
     GetParam
     SetParam
     GetConfig
     DownloadPatch
     GetRateData
     IspRequest
     ProgramFlash
     EraseFlash
     LedStates
     LedEvent
     AbortPrint
     Spare2
     Spare3
     Spare4
     Peek
     Poke
     SensorSpiTrans
     SensorGPIO
     GetFingerState 
));

sub cmd_id {
	my ($packet) = @_;
	$packet =~ m/^(..) 00/;
	hex($1);
}

sub cmd_name {
	$cmd[cmd_id $_[0]] or ""
}

sub is_cmd {
	defined $cmd[cmd_id $_[0]]
}



#-----------------------------------------------------------------------------------
# Input file handling
#-----------------------------------------------------------------------------------

sub grab {
	my ($type) = @_;
	my @packet;

	# move all matching lines to temp array
	push @packet, next_line while looking_at $type;

	# strip the line headers
	map { $_ =~ s/^$type: // } @packet;

 	# join into a single string
	join " ", @packet;
}

sub strip {
	my ($line) = @_;
	warn "Can't strip seq" unless $line =~ s/^.. .. 00 00 //;
	$line;
}



#-----------------------------------------------------------------------------------
# Argument list formatters for SEND output routine
#-----------------------------------------------------------------------------------

sub fmt_none {
	""
}

sub fmt_unknown {
	", ERROR"
}

sub fmt_GetPrint {
	my %type = (
		"00 01 00 00 00 01" => "type_0",
		"01 00 00 00 01 01" => "type_1",
		"00 00 00 00 00 01" => "type_2",
	);
	$_[0] =~ m/03 00 (..) (..) (.. .. .. .. .. ..)$/;
	", 0x$2$1, " . ($type{$3} or "type_UNKNOWN($3)");
}

sub fmt_GetParam {
	$_[0] =~ m/04 00 (..) (..)$/;
	", 0x$2$1";
}

sub fmt_SetParam {
	$_[0] =~ m/05 00 (..) (..) (..) (..)$/;
	", 0x$2$1, 0x$4$3";
}

sub fmt_Peek {
	$_[0] =~ m/12 00 (..) (..) (..) (..) (..)$/;
	", 0x$4$3$2$1, 0x$5";
}

sub fmt_Poke {
	$_[0] =~ m/13 00 (..) (..) (..) (..) (..) (..) (..) (..) (..)$/;
	", 0x$4$3$2$1, 0x$8$7$6$5, 0x$9";
}

sub fmt_SensorSpiTrans {
	$_[0] =~ m/14 00 (..) (..) (..) (..) (..) (..) (..)$/;
	", 0x$1, 0x$2, 0x$3, 0x$4, 0x$5, 0x$6, 0x$7";
}

my @fmt = (undef,
	\&fmt_none,	#  Reset
	\&fmt_none,	#  GetVersion
	\&fmt_GetPrint,	#  GetPrint
	\&fmt_GetParam,	#  GetParam
	\&fmt_SetParam,	#  SetParam
	\&fmt_none,	#  GetConfig
	\&fmt_unknown,	#  DownloadPatch
	\&fmt_unknown,	#  GetRateData
	\&fmt_unknown,	#  IspRequest
	\&fmt_unknown,	#  ProgramFlash
	\&fmt_unknown,	#  EraseFlash
	\&fmt_unknown,	#  LedStates
	\&fmt_unknown,	#  LedEvent
	\&fmt_none,	#  AbortPrint
	\&fmt_unknown,	#  Spare2
	\&fmt_unknown,	#  Spare3
	\&fmt_unknown,	#  Spare4
	\&fmt_Peek,	#  Peek
	\&fmt_Poke,	#  Poke
	\&fmt_SensorSpiTrans,	#  SensorSpiTrans
	\&fmt_unknown,	#  SensorGPIO
	\&fmt_none,	#  GetFingerState
);

sub fmt_args {
	$fmt[cmd_id $_[0]] or fmt_unknown
}



#-----------------------------------------------------------------------------------
# Produce output file from input array
#-----------------------------------------------------------------------------------

my $seq;
my $max_seq = -1;
my $last_cmd;


sub dump_send_1 {
	my $packet = strip grab "SEND";
	$last_cmd = cmd_id $packet;
	print "\t_(  " . cmd_name($packet) . " (dev" . fmt_args($packet)->($packet) . "));\n";
}

sub dump_send_2 {
	my $packet = strip grab "SEND";
	$last_cmd = cmd_id $packet;
	printf "\t__(%5d,    " . cmd_name($packet) . " (dev" . fmt_args($packet)->($packet) . "));\n", $seq;
}

sub dump_send_3 {
	my $packet = strip grab "SEND";
	$last_cmd = cmd_id $packet;
}

sub dump_recv_3 {
	my $packet = strip grab "RECV";
	warn "Response type mismatch..." unless $last_cmd == cmd_id $packet;
	my @p2 = split /\s+/, $packet;
	my $p2 = join "\\x", @p2;
	printf "\t[ %5d ] = { %3d, \"\\x$p2\" },\n", $seq, $#p2+1;
}

sub dump_pnm {
	my $img = grab "LOAD";
	return unless length $img;
	my @img = map { hex $_ } split " ", $img;
	my $n = $#img + 1;
	warn "bad img len $n" if $n % 292;
	my $y = $n / 292;
	open PNM, sprintf ">img2/%05d.pnm", $seq;
	print PNM "P2\n292 $y\n256\n";
	while ($#img >= 291) {
		my (@i) = splice(@img, 0, 292);
		print PNM join " ", map {sprintf "%3d", $_ } @i;
		print PNM "\n";
	}
	close PNM;
}

sub dump_load_1 {
	print "\t_(  LoadImage (dev));\n";
	drop "LOAD";
}

sub dump_load_2 {
	print "\t _(          LoadImage (dev));\n";
	dump_pnm;
}

sub dump_time {
	my ($t) = next_line;
	$t =~ m/^TIME: (\d+) (\d+)/;
	$seq = $2;
	$max_seq = $seq if $seq > $max_seq;
}

sub output_unchecked {
	if (looking_at "SEND") {
		dump_send_1;

	} elsif (looking_at "RECV") {
		next_line;

	} elsif (looking_at "LOAD") {
		dump_load_1;

	} elsif (looking_at "TIME") {
		next_line;

	} else {
		warn next_line;
	}
}

sub output_checked {
	if (looking_at "SEND") {
		dump_send_2;

	} elsif (looking_at "RECV") {
		next_line;

	} elsif (looking_at "LOAD") {
		dump_load_2;

	} elsif (looking_at "TIME") {
		dump_time;

	} else {
		warn next_line;
	}
}

sub output_results {
	if (looking_at "SEND") {
		dump_send_3;

	} elsif (looking_at "RECV") {
		dump_recv_3;

	} elsif (looking_at "LOAD") {
		next_line;

	} elsif (looking_at "TIME") {
		dump_time;

	} else {
		warn next_line;
	}
}

sub process_1 {
	first_line;
	&{$_[0]} while more_lines;
}

sub process_file {

	print "int PREFIX_unchecked (struct vfs_dev *dev)\n";
	print "{\n";
	process_1 \&output_unchecked;
	print "\treturn 0;\n";
	print "}\n";

	print "int PREFIX_checked (struct vfs_dev *dev)\n";
	print "{\n";
	process_1 \&output_checked;
	print "\treturn 0;\n";
	print "}\n";

	print "struct result_table PREFIX_results =\n";
	print "{\n";
	print "\t$max_seq,\n";
	print "\t{\n";
	process_1 \&output_results;
	print "\t}\n";
	print "};\n";
}

process_file;
