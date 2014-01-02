#!/usr/bin/perl
use strict;
use warnings;

our $VERSION = 0.01;

use File::Temp qw( tempdir );

use Test::More;
use Test::Mojo;

use FindBin;
## no critic RequireBarewordIncludes
require "$FindBin::Bin/../../xdeadly";

## no critic RequireBarewordIncludes
use lib "$FindBin::Bin/../lib";
use XDeadlyFixtures;

my $dir = tempdir( CLEANUP => 1 );
XDeadlyFixtures::copy_good_data_fixtures($dir);

my $t = Test::Mojo->new;
$t->app->config( data_dir => $dir );

$t->get_ok('/summary.rss')->status_is(200);

$t->text_like(
    'rss > channel > item:nth-of-type(1) > link',
    qr{http://localhost:\d+/article/19700101030000}
);
$t->text_like(
    'rss > channel > item:nth-of-type(2) > link',
    qr{http://localhost:\d+/article/19700101020000}
);
$t->text_like(
    'rss > channel > item:nth-of-type(3) > link',
    qr{http://localhost:\d+/article/19700101010000}
);
$t->element_exists_not( 'rss > channel > item:nth-of-type(4)' );

foreach my $entry (1,2,3) {
    my $path = "rss > channel > item:nth-of-type($entry)";
    $t->element_exists($path);
    foreach my $key (qw(
        title
        link
        category
        pubDate
        description
    )) {
        $t->element_exists("$path > $key");
    }
}

$t->element_exists_not($_) for qw(
    id
    subject
    department
    date
    name
    host
    has_more
);

done_testing();
