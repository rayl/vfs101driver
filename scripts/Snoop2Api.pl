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

my $want_a_load = 0;
my $cmd;

sub dump_send {
	print "	// LoadImage();  // Why no call here?\n" if $want_a_load;
	my $packet = strip grab "SEND";
	$cmd = cmd_id $packet;
	print "\n	" . cmd_name($packet) . "();\n";
	$want_a_load = 0;
}

sub dump_recv {
	print "	// LoadImage();\n" if $want_a_load;
	my $packet = strip grab "RECV";
	warn "Response type mismatch..." unless $cmd == cmd_id $packet;
	print "	// expect: \"$packet\"\n";
	$want_a_load = ($cmd == 0x03) || ($cmd == 0x0e) || (($cmd == 0x16) && ($packet =~ m/ 02$/));
}

sub dump_load {
	drop "LOAD";
	print $want_a_load
		? "	LoadImage();\n"
		: "	LoadImage();  // UNEXPECTED call, why is this here?\n";
	$want_a_load = 0;
}

sub process_file {

	while (more_lines) {

		if (looking_at "SEND") {
			dump_send;

		} elsif (looking_at "RECV") {
			dump_recv;

		} elsif (looking_at "LOAD") {
			dump_load;

		}
	}
}

process_file;
