#!/usr/bin/perl
use warnings;
use strict;
use TAP::Harness;
my $harness = TAP::Harness->new({
        timer => 1,
        color => 1,
	exec => sub {
		my ($harness, $test_file) = @_;
		# Let Perl tests run.
		return undef if $test_file =~ /[.].pl$/;
		if (!-f $test_file) {
		  die "File '$test_file' doesn't exists!\n";
		}
		if (-x $test_file) {
			return [ $test_file ];
		}
		die "File '$test_file' isn't executable!\n";
	},
});
my $aggregator = $harness->runtests(@ARGV);
exit($aggregator->exit);