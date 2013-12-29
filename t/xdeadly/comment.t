#!/usr/bin/perl
use Mojo::Base -strict;
use Test::More;
use File::Temp qw/ tempdir /;

use lib 't/lib';
use XDeadlyFixtures;

use XDeadly::Comment;
my $dir = tempdir( CLEANUP => 1 );
XDeadlyFixtures::copy_good_data_fixtures($dir);

my $comment = XDeadly::Comment->new;

is $comment->filename, 'comment', 'A new comment has the right filename';

is $comment->id,     undef, 'A new comment has no id';
is $comment->parent, undef, 'A new comment has no parent';
is $comment->cid,    undef, 'A comment without a parent has no cid';

my $article = XDeadly::Comment->articles($dir)->[-1];

is $article->id, '19700101010000', 'Loaded the right article';

ok my $first_comment = $article->comments->[0], 'Loaded the first comment';
is $first_comment->data_dir, $dir, 'Comment has correct data_dir';
is $first_comment->id, '19700101010000/1', 'First comment has the correct id';
is $first_comment->cid, '1', 'First comment has correct cid';
is $first_comment->article, $article, 'Comment has correct article';
is $first_comment->parent,  $article, 'Comment has correct parent';

is $first_comment->level, 1, 'Comment at level 1 has correct level';

my $parent = $article;
my @tests  = (
    {   id    => '19700101010000/4',
        cid   => '4',
        level => 1,
    },
    {   id    => '19700101010000/5',
        cid   => '5',
        level => 1,
        make_parent => 1,
    },
    {   id    => '19700101010000/5/1',
        cid   => '1',
        level => 2,
    },
    {   id    => '19700101010000/5/2',
        cid   => '2',
        level => 2,
        make_parent => 1,
    },
    {   id    => '19700101010000/5/2/1',
        cid   => '1',
        level => 3,
    },
);

foreach my $t (@tests) {
    my $id = $t->{id};
    ok $comment = XDeadly::Comment->new( parent => $parent ), "[$id] New";

    is $comment->data_dir, $dir,     "[$id] correct data_dir";
    is $comment->article,  $article, "[$id] correct article";
    is $comment->parent,   $parent,  "[$id] correct parent";

    $parent = $comment if delete $t->{make_parent};

    foreach my $method ( sort keys %{$t} ) {
        is $comment->$method, $t->{$method},
            "[$id] correct $method [$t->{$method}]";
    }
}

done_testing;
