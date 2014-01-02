#!/usr/bin/perl
use Mojo::Base -strict;
use Test::Mojo;
use Test::More;
use File::Temp qw/ tempdir /;

use FindBin;
## no critic RequireBarewordIncludes
require "$FindBin::Bin/../../xdeadly";

## no critic RequireBarewordIncludes
use lib "$FindBin::Bin/../lib";
use XDeadlyFixtures;

use XDeadly::Comment;
my $dir = tempdir( CLEANUP => 1 );
XDeadlyFixtures::copy_good_fixtures($dir);


my $t = Test::Mojo->new;
$t->app->config( data_dir => "$dir/data" );

# Get the homepage
$t->get_ok('/')->status_is(200);
$t->content_like(qr/OpenBSD Journal: A resource for the OpenBSD community/);

# Read an article
follow_link_ok( $t, '/article/19700101010000' );
$t->content_like( qr/Test Post #1/, 'Got the right page' );
$t->content_like( qr/Contributed by.*Test McTesterson/, 'By the right author' );

# Flatten the comments
follow_link_ok( $t, '/article/19700101010000?mode=flat' );

# Expand the comments
follow_link_ok( $t, '/article/19700101010000?mode=expanded' );

# View a comment
follow_link_ok( $t, '/article/19700101010000/1/2' );
$t->content_like( qr/Test Comment #1-1-2/ );

# Add a story as anonymous
# check the homepage that the story isn't there
# login as an admin, approve the story
# logout as the admin
# Check the story is on the homepage as anonymous
# Mod a comment up as anonymous
# Mod a comment down as anonymous
# Mod a comment neutral as anonymous
#
# register as a new user
#
# login as the new user, add a story
# Do all the things we tried as anonymous as the new user
# Change new user's preferences
# Somehow check that we expire sessions

done_testing;

sub follow_link_ok {
    my ($t, $href, @args ) = @_;

    my $link = $t->tx->res->dom->at("a[href^=$href]");
    return fail "Couldn't find $href" unless $link;
    $t->get_ok( $link->attr('href'), @args );
}
