#!/usr/bin/env perl
#
# We need to have some reasonable assurance that the errata
# file we have exists and is being parsed.

use strict;
use warnings;

use Test::Simple tests => 1;

use FindBin qw($Bin);
print "$Bin/../../public/errata.json\n";
ok(-r "$Bin/../../public/errata.json", "Scraped errata file exists.");

