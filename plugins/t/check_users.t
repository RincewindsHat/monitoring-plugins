#! /usr/bin/perl -w -I ..
#
# Logged in Users Tests via check_users
#
# Trick: This check requires at least 1 user logged in. These commands should
#        leave a session open forever in the background:
#
#   $ ssh -tt localhost </dev/null >/dev/null 2>/dev/null &
#   $ disown %1

use strict;
use Test;
use NPTest;

use vars qw($tests);
BEGIN {$tests = 12; plan tests => $tests}

my $successOutput = '/[0-9]+ users currently logged in/';
my $failureOutput = '/[0-9]+ users currently logged in/';
my $wrongOptionOutput = '/Usage:/';

my $t;

$t += checkCmd( "./check_users 1000 1000", 0, $successOutput );
$t += checkCmd( "./check_users    0    0", 2, $failureOutput );
$t += checkCmd( "./check_users -w 0:1000 -c 0:1000", 0, $successOutput );
$t += checkCmd( "./check_users -w 0:0 -c 0:0", 2, $failureOutput );
$t += checkCmd( "./check_users -w 0:1000", 3, $wrongOptionOutput);
$t += checkCmd( "./check_users", 3, $wrongOptionOutput);

exit(0) if defined($Test::Harness::VERSION);
exit($tests - $t);

