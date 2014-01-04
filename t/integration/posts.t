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
XDeadlyFixtures::copy_good_data_fixtures($dir);

my $t = Test::Mojo->new;
$t->app->config( data_dir => $dir );

my @sids = (    # in order
    '19700101030000',
    '19700101020000',
    '19700101010000',
);

my %comments = (
    '19700101010000' => [
        '19700101010000/1',     '19700101010000/1/1',
        '19700101010000/1/2',   '19700101010000/2',
        '19700101010000/3',     '19700101010000/3/1',
        '19700101010000/3/1/1', '19700101010000/3/1/2',
    ],
    '19700101010000/3' => [
        '19700101010000/3/1', '19700101010000/3/1/1',
        '19700101010000/3/1/2',
    ],
    '19700101010000/3/1' =>
        [ '19700101010000/3/1/1', '19700101010000/3/1/2', ],
    '19700101010000/3/1/1' => [],
    '19700101020000'       => [],
    '19700101030000'       => [],
);

my @stories = $t->app->stories;
is @stories, @sids, 'Correct number of stories';

for ( my $i = 0; $i < @stories; $i++ ) {
    my $story = $stories[$i];
    my $sid   = $sids[$i];

    is $story, $sid, "[$story] Correct story";
    ok $story->isa('XDeadly::Article'), "[$story] ISA XDeadly::Article";
}

foreach my $id ( sort keys %comments ) {
    my $post = $t->app->story($id);
    is $post, $id, "[$post] Correct post";
    ok $post->isa('XDeadly::Post'), "[$post] ISA XDeadly::Post";

    my $c = $post->comments;
    is @{$c}, @{ $comments{$id} }, "[$post] Correct number of comments";

    for ( my $i = 0; $i < @{ $comments{$id} }; $i++ ) {
        my $comment = $c->[$i];
        my $cid     = $comments{$id}[$i];

        is $comment, $cid, "[$post][$comment] Correct Comment";
        ok $comment->isa('XDeadly::Comment'),
            "[$post][$comment] ISA XDeadly::Comment";
    }
}

done_testing;
