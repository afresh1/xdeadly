#!/usr/bin/perl
use Mojo::Base -strict;
use Test::Mojo;
use Test::More;

use FindBin;
## no critic RequireBarewordIncludes
require "$FindBin::Bin/../../xdeadly";

my $t = Test::Mojo->new;

$t->get_ok("/submissions")
    ->status_is(404);

done_testing();
