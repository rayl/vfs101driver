#!/usr/bin/perl

# UsbSnoop logfile parser
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



# All lines from the UsbSnoop tracefile. The current line being processed
# sits on the top of the array and is popped off when processing is complete.
my @line;
chomp(@line = <>);

# are there any lines left to process?
sub more_lines {
	defined $line[0]
}

# return the current line
sub current_line {
	$line[0]
}

# go to next line, returning the current one
sub next_line {
	shift @line
}

# does the current line contain this pattern?
sub looking_at {
	more_lines and $line[0] =~ m/$_[0]/
}





# are we at the start of a new block?
sub at_new_block {
	(not more_lines) or (looking_at "^\\[");
}

# skip to the start of the next block
sub next_block {
	print "        ********** FLUSHING!! ***********\n";
	while (not at_new_block) {
		print "        Flush: " . next_line . "\n";
	}
}





sub get_seq {
	current_line =~ m/.* URB (\d+) .*/;
	return $1;
}

sub get_type {
	current_line =~ m/^-- (.*):$/;
	return $1;
}

sub get_endpoint {
	current_line =~ m/^  PipeHandle           = ........ \[endpoint 0x000000(..)\]$/;
	return $1;
}





# convert a list into a hash map
sub make_set {
	map { $_ => 1 } @_
}

# determine if a block is a bulk transfer, warning about unknown types
sub is_bulk {
	my ($type) = @_;
	my %bulk = make_set (
		"URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER"
	);
	my %nonbulk = make_set (
		"URB_FUNCTION_CONTROL_TRANSFER",
		"URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE",
		"URB_FUNCTION_SELECT_CONFIGURATION",
		"URB_FUNCTION_SET_FEATURE_TO_DEVICE",
	);
	return 1 if $bulk{$type};
	return 0 if $nonbulk{$type};
	warn "Unknown block: $type";
	return 0;
}

# determine if an endpoint is valid
sub is_valid {
	my ($ep) = @_;
	my %valid = make_set (
		"01",
		"81",
		"82",
	);
	return 1 if $valid{$ep};
	warn "Endpoint $ep is not valid...\n";
	return 0;
}
 
sub handle_bulk {
	my ($label) = @_;

	# determine the block type
	next_line;
	my $type = get_type;
	return next_block unless is_bulk $type;
	print "      $type\n";

	# check which endpoint
	next_line;
	my $ep = get_endpoint;
	return next_block unless is_valid $ep;
	print "      $label: $ep\n";

	# ignore next four lines
	next_line;
	return next_block unless looking_at "TransferFlags";
	# extract dir, compare to ep

	next_line;
	return next_block unless looking_at "TransferBufferLength";
	# extract length, save for parsing data

	next_line;
	return next_block unless looking_at "TransferBuffer";

	next_line;
	return next_block unless looking_at "TransferBufferMDL";

	# handle any data transfers
	next_line;
	while (looking_at "^    0") {
		$line[0] =~ m/^ +[0-9a-f]{8}: (.*)/;
		print "            $1\n";
		next_line;
	}

	return next_block unless looking_at "UrbLink";
}

# sequence number of current swap
my $seq = 1;

sub seq_request {
	my $s = get_seq;
	print "\n\n==============================================================================\n----> $s\n";
	warn "Sequence discontinuity, expecting $seq, jumped to $s instead" unless $seq == $s;
	$seq = $s;
}

sub seq_response {
	my $s = get_seq;
	print "<---- $s\n";
	warn "Sequence mismatch, got response $s to request $seq" unless $seq == $s;
	$seq++;
}



# process the contents of the log file
sub process_log_file {

	# deal with each line in order
	while (more_lines) {

		# make sure we are at the start of a new section
		if (not at_new_block) {
			print "Alignment problem!\n";
			next_block;

		# we don't care about internal UsbSnoop sections
		} elsif (looking_at "UsbSnoop") {
			next_line;

		# but we do care about USB requests...
		} elsif (looking_at ">>>") {
			seq_request;
			handle_bulk "SEND";
			next_line;

		# ...and USB responses
		} elsif (looking_at "<<<") {
			seq_response;
			handle_bulk "RECV";
			next_line;

		# flag anything else for manual investigation
		} else {
			warn "Unrecognized line: " . next_line . "\n";

		}
	}
}

process_log_file;
