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

# all lines from the UsbSnoop tracefile
my @line;

# sequence number of current swap
my $seq = 1;

# process the contents of the log file
sub process_log_file {

	# deal with each line in order
	while (my $line = shift @line) {

		# make sure we are at the start of a new section
		if ($line !~ m/^\[/) {
			print "Alignment problem: $line\n";

		# we don't care about internal UsbSnoop sections
		} elsif ($line =~ m/UsbSnoop/) {
			#print "Ignoring: $line\n";

		# but we do care about USB requests...
		} elsif ($line =~ m/>>>/) {
			seq_request($line);
			handle_bulk("SEND");

		# ...and USB responses
		} elsif ($line =~ m/<<</) {
			seq_response($line);
			handle_bulk("RECV");

		# flag anything else for manual investigation
		} else {
			warn "Unrecognized block header: $line\n";

		}
	}
}

sub seq_request {
	my ($line) = @_;

	# process sequence number
	my $s = get_seq($line);
	print "\n\n==============================================================================\n----> $s\n";
	if ($seq != $s) {
		warn "Sequence discontinuity, expecting $seq, jumped to $s instead";
	}
	$seq = $s;
}

sub seq_response {
	my ($line) = @_;

	# process sequence number
	my $s = get_seq($line);
	print "<---- $s\n";
	if ($seq != $s) {
		warn "Sequence mismatch, got response $s to request $seq";
	}
	$seq++;
}

sub flush {
	print "        ********** FLUSHING!! ***********\n";
	while ($line[0] !~ m/^\[/) {
		print "        Flush: " . shift(@line) . "\n";
	}
}

sub skip {
	my ($word) = @_;
	my $line = shift @line;
	return 0 unless $line =~ m/\s$word\s/;
	return 1;
}

sub handle_bulk {
	my ($label) = @_;
	my ($line);

	# determine the block type
	$line = shift @line;
	my $type = get_type($line);
	print "      $type\n";
	return flush unless is_bulk($type);

	# check which endpoint
	$line = shift @line;
	my $ep = get_endpoint($line);
	print "      $label: $ep\n";
	return flush unless is_valid($ep);

	# skip next four lines
	return flush unless skip "TransferFlags";		# extract dir, compare to ep
	return flush unless skip "TransferBufferLength";	# length, save for parsing data
	return flush unless skip "TransferBuffer";
	return flush unless skip "TransferBufferMDL";

	# handle any data transfers
	while ($line[0] =~ m/^    0/) {
		$line = shift @line;
		$line =~ s/^ +[0-9a-f]{8}: //;
		print "            $line\n";
	}

	return flush unless skip "UrbLink";
}

sub get_seq {
	my ($line) = @_;
	$line =~ m/.* URB (\d+) .*/;
	return $1;
}

sub get_type {
	my ($line) = @_;
	$line =~ m/^-- (.*):$/;
	return $1;
}

sub get_endpoint {
	my ($line) = @_;
	$line =~ m/^  PipeHandle           = ........ \[endpoint 0x000000(..)\]$/;
	return $1;
}

sub is_valid {
	my ($ep) = @_;
	my %valid = (
		"01" => 1,
		"81" => 1,
		"82" => 1,
	);
	return 1 if $valid{$ep};
	warn "Endpoint $ep is not valid...\n";
	return 0;
}
 
sub is_bulk {
	my ($type) = @_;

	if ($type eq "URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER") {
		return 1;

	} elsif ($type eq "URB_FUNCTION_CONTROL_TRANSFER") {
		return 0;

	} elsif ($type eq "URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE") {
		return 0;

	} elsif ($type eq "URB_FUNCTION_SELECT_CONFIGURATION") {
		return 0;

	} elsif ($type eq "URB_FUNCTION_SET_FEATURE_TO_DEVICE") {
		return 0;

	} else {
		warn "Unknown block: $type";
		return 0;
	}
}

chomp(@line = <>);
process_log_file;
