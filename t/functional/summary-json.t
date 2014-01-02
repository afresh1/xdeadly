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

$t->get_ok('/summary.json')->status_is(200);

$t->json_is( '/0/id', '19700101030000', 'entry 0 has correct id' );
$t->json_is( '/1/id', '19700101020000', 'entry 0 has correct id' );
$t->json_is( '/2/id', '19700101010000', 'entry 0 has correct id' );
$t->json_hasnt( '/3', 'Only 3 entries' );

foreach my $entry ( 0, 1, 2 ) {
    $t->json_has("/$entry");
    foreach my $key ( qw(
        body
        comments
        department
        host
        id
        link
        mod
        name
        pubdate
        subject
    ) ) {
        $t->json_has("/$entry/$key");
    }
}

$t->json_is( '/2/comments/1/mod', { score => 0, votes => 0 } );
$t->json_is( '/2/comments/-1/id', '19700101010000/3/1/2' );
$t->json_hasnt('/2/comments/0/comments');

done_testing();
