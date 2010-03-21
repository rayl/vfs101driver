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
my @line;
chomp(@line = <>);

# return the current line
sub current_line {
	$line[0]
}

# go to next line, returning the current one
sub next_line {
	shift @line
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
my $last_cmd;
my $timestamp = 0;


sub dump_send {
	my $packet = strip grab "SEND";
	$last_cmd = cmd_id $packet;
	printf "__(%5d,    " . cmd_name($packet) . " (dev" . fmt_args($packet)->($packet) . "));\n", $seq;
}

sub dump_recv {
	my $packet = strip grab "RECV";
	warn "Response type mismatch..." unless $last_cmd == cmd_id $packet;
}

sub dump_load {
	drop "LOAD";
	printf "__(%5d,    LoadImage (dev));\n", $seq;
}

sub dump_time {
	my ($t) = next_line;
	$t =~ m/^TIME: (\d+) (\d+)/;
	$seq = $2;
	return;
	if ($1 > $timestamp) {
		print "usleep(" . (($t - $timestamp) * 1000) . ");\t\t\t\t\t// $t ms\n";
		$timestamp = $t;
	}
}

sub output_unchecked {

	while (more_lines) {

		if (looking_at "SEND") {
			dump_send;

		} elsif (looking_at "RECV") {
			dump_recv;

		} elsif (looking_at "LOAD") {
			dump_load;

		} elsif (looking_at "TIME") {
			dump_time;

		}
	}
}

sub output_checked {
}

sub output_results {
}


sub process_file {
	output_unchecked;
	output_checked;
	output_results;
}

process_file;
