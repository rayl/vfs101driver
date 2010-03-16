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




my $want_a_load = 0;

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

sub dump_send {
	warn "Didn't get a LOAD..." if $want_a_load;
	my $packet = strip grab "SEND";
	print "\n	send($packet);\n";
	$want_a_load = 0;
}

sub dump_recv {
	warn "Didn't get a LOAD..." if $want_a_load;
	my $packet = strip grab "RECV";
	print "	// expecting \"$packet\"\n";
	$want_a_load = 0; # 03 or 0e or 16/02
}

sub dump_load {
	#warn "Unexpected LOAD...." unless $want_a_load;
	drop "LOAD";
	print "	LoadImage();\n";
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
