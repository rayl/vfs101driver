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



sub drop {
	next_line while looking_at $_[0];
}

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

my $cmd;

sub fmt_none {
	""
}

sub fmt_unknown {
	", ERROR"
}

sub fmt_GetPrint {
	my ($packet) = @_;
	", COUNT, type_X"
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

my @fmt = (undef,
	[ \&fmt_none,		3 ],	#  Reset
	[ \&fmt_none,		4 ],	#  GetVersion
	[ \&fmt_GetPrint,	2 ],	#  GetPrint
	[ \&fmt_GetParam,	3 ],	#  GetParam
	[ \&fmt_SetParam,	2 ],	#  SetParam
	[ \&fmt_none,		4 ],	#  GetConfig
	[ \&fmt_unknown,	3 ],	#  DownloadPatch
	[ \&fmt_unknown,	3 ],	#  GetRateData
	[ \&fmt_unknown,	3 ],	#  IspRequest
	[ \&fmt_unknown,	3 ],	#  ProgramFlash
	[ \&fmt_unknown,	3 ],	#  EraseFlash
	[ \&fmt_unknown,	3 ],	#  LedStates
	[ \&fmt_unknown,	3 ],	#  LedEvent
	[ \&fmt_none,		4 ],	#  AbortPrint
	[ \&fmt_unknown,	3 ],	#  Spare2
	[ \&fmt_unknown,	3 ],	#  Spare3
	[ \&fmt_unknown,	3 ],	#  Spare4
	[ \&fmt_Peek,		2 ],	#  Peek
	[ \&fmt_Poke,		1 ],	#  Poke
	[ \&fmt_unknown,	2 ],	#  SensorSpiTrans
	[ \&fmt_unknown,	3 ],	#  SensorGPIO
	[ \&fmt_none,		3 ],	#  GetFingerState
);

sub dump_args {
	$fmt[cmd_id $_[0]]->[0] or fmt_unknown
}

sub n_tabs {
	$fmt[cmd_id $_[0]]->[1] or 4
}

sub dump_send {
	my $packet = strip grab "SEND";
	$cmd = cmd_id $packet;
	print "	_(  " . cmd_name($packet) . " (dev" . dump_args($packet)->($packet) . "));";
}

sub dump_recv {
	my $packet = strip grab "RECV";
	warn "Response type mismatch..." unless $cmd == cmd_id $packet;
	print "\t" x n_tabs($packet);
	print "// expect: \"$packet\"\n";
}

sub dump_load {
	drop "LOAD";
	print "	_(  LoadImage (dev));\n"
}

my $timestamp = 0;

sub dump_time {
	my ($t) = next_line;
	$t =~ s/^TIME: //;
	if ($t > $timestamp) {
		print "	usleep(" . (($t - $timestamp) * 1000) . ");\t\t\t\t\t// $t ms\n";
		$timestamp = $t;
	}
}

sub process_file {

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

process_file;
