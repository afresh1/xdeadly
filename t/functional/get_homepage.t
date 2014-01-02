#!/usr/bin/perl
use strict;
use warnings;

our $VERSION = 0.01;

use Test::More;
use Test::Mojo;

use FindBin;
## no critic RequireBarewordIncludes
require "$FindBin::Bin/../../xdeadly";

my $t = Test::Mojo->new;

$t->get_ok('/')->status_is(200);
$t->content_like(qr/OpenBSD Journal: A resource for the OpenBSD community/);

TODO: { local $TODO = 'Someday they will all be gone!';
$t->content_unlike(qr/XXX FIXME/);
}

done_testing();
