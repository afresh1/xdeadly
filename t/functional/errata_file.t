#!/usr/bin/env perl
#
# We need to have some reasonable assurance that the errata
# file we have exists and is being parsed.

use strict;
use warnings;

use Test::More;
use Test::Mojo;

use FindBin qw($Bin);

my $errata_file_path = "$Bin/../../public/errata.json";
print "$errata_file_path\n";
ok(-r $errata_file_path, "Scraped errata file exists.");

# Can we decode the file?
use Mojo::Asset::File;
use Mojo::JSON qw(decode_json);
my $errata_file = Mojo::Asset::File->new(path => $errata_file_path);
my $errata_json;
ok( $errata_json = decode_json($errata_file->slurp) , 'Decoding JSON file succeeds.');

# Get the title that should be on top.
my @rev_errata = reverse @{$errata_json};
my $eitem = (keys %{$rev_errata[0]})[0];
my $first_title = $rev_errata[0]->{$eitem}->[0]->{'title'};

require "$Bin/../../xdeadly";
my $t = Test::Mojo->new;

# Test that the top title is on the page.
$t->get_ok('/')->status_is(200);
$t->content_like(qr/$first_title/);

done_testing();
