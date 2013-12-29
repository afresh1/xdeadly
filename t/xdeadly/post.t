#!/usr/bin/perl
use Mojo::Base -strict;
use Test::More;

use File::Temp qw/ tempdir /;
use File::Path qw/ make_path /;
use File::Spec;

use XDeadly::Post;

use lib 't/lib';
use XDeadlyFixtures;

my $dir = tempdir( CLEANUP => 1 );

my $message_text = q{name: Testy McTesterson
email: test@example.com
href: example.com
date: Tue Aug 20 14:51:37 2013
host: localhost
subject: This is only a test

If this had been an actual post you would have been asked to evacuate.
};

my %headers = (
    Name    => 'Testy McTesterson',
    EMail   => 'test@example.com',
    HREF    => 'example.com',
    Date    => 'Tue Aug 20 14:51:37 2013',
    Host    => 'localhost',
    Subject => 'This is only a test',
);
my $body = "If this had been an actual post you would have been asked to evacuate.\x0a";

ok my $post = XDeadly::Post->new;
is $post->data_dir, undef, 'new post has no data_dir';
is $post->id, undef, 'new post has no id';

is $post->level, 0, 'Posts have a level of 0';

ok $post = XDeadly::Post->new( data_dir => $dir, id => 'test' );
is $post->data_dir, $dir, 'Got a data_dir';
is $post->id, 'test', 'Got an id';
is $post->dir, "$dir/test";
is $post->path, "$dir/test/post";

{
    mkdir "$dir/loaded" or die;
    open my $fh, '>', "$dir/loaded/post" or die $!;
    print $fh $message_text;
    close $fh;
}

ok my $loaded = XDeadly::Post->new( data_dir => $dir, id => 'loaded' );
ok $loaded->has_path, 'loaded post has a path';
is $loaded->data_dir, $dir, 'loaded post has correct data_dir';
is $loaded->dir, "$dir/loaded", 'loaded post has correct dir';
is $loaded->path, "$dir/loaded/post", 'loaded post has correct path';
is $loaded->id, 'loaded', 'loaded post has corret id';
is $loaded->level, 0, 'loaded post has correct level';
ok !$loaded->is_article, 'loaded post is not an article';
is $loaded->filename, 'post', 'loaded post filename is post';

ok !$loaded->{content}, 'Still have not loaded the content for the post';

is $loaded->build_body, $body, 'Loaded body matches';
is $loaded->headers->header($_), $headers{$_}, "Loaded header($_) matches"
    for keys %headers;

ok $post = XDeadly::Post->new;
ok $post->parse($message_text), 'Created a test post';
is $post->id, undef, 'parsed post has no id';

is $post->_epoch, 1377010297, 'correctly parsed the epoch';

is $post->build_body, $body, 'Body matches';
is $post->headers->header($_), $headers{$_}, "Header($_) matches"
    for keys %headers;

ok $post->save( data_dir => $dir, id => 'test' ), 'saved post';
is $post->path, "$dir/test/post", 'post knows where it is';

my $reloaded = XDeadly::Post->new( data_dir => $dir, id => 'test' );
is $reloaded->path, "$dir/test/post", 'Reloaded has correct path';
is $post->dir, "$dir/test", 'Reloaded has correct dir';

is $post->build_body, $reloaded->build_body, 'Reloaded body matches';
is $reloaded->headers->header($_), $headers{$_}, "Reloaded header($_) matches"
    for keys %headers;

$post = XDeadly::Post->new;
ok open( my $fh, '<', "$dir/test/post" ), 'Opened post';
ok $post->parse( do { local $/ = undef; <$fh> } ), 'manual load parsed';
close $fh;

is $post->build_body, $body, 'manual load body matches';
is $post->headers->header($_), $headers{$_}, "manual load Header($_) matches"
    for keys %headers;

ok $post->data_dir($dir), 'set post data_dir';
ok $post->id('test2'), 'set post id';
ok $post->save, 'save post to ->path';

$reloaded = XDeadly::Post->new();
ok $reloaded->data_dir( $dir ), 'set data_dir after ->new';
ok $reloaded->id( 'test2' ), 'set id after ->new';

is $post->build_body, $reloaded->build_body, 'Reloaded body matches';
is $reloaded->headers->header($_), $headers{$_}, "Reloaded header($_) matches"
    for keys %headers;

eval { XDeadly::Post::_parse_ctime('Thu Jan 01 00:00:00 UTC 1970') };
ok $@ =~ /Invalid date/, 'Date with a timezone is invalid';

XDeadlyFixtures::copy_good_data_fixtures($dir);

make_path(
    File::Spec->catdir( $dir, '19700202020202' ),
    File::Spec->catdir( $dir, '19700101000100', '5' )
);

ok my $articles = XDeadly::Article->articles($dir);
is @{ $articles }, 3, 'loaded 3 articles';
is_deeply [ map { $_->id } @{$articles} ],
    [ '19700101030000', '19700101020000', '19700101010000', ],
    'got ids in the correct order';

ok(!$_->{content}, 'Loading and getting article id did not load content')
    for @{ $articles };

ok( $_->isa('XDeadly::Post'),    "$_ isa XDeadly::Post" )    for @{$articles};
ok( $_->isa('XDeadly::Article'), "$_ isa XDeadly::Article" ) for @{$articles};

is_deeply [ map { $_->_epoch } @{$articles} ],
    [ '180', '120', '60', ],
    'got epoch in the correct order';

ok($_->{content}, 'Getting article epoch did load content')
    for @{ $articles };


my $article = $articles->[-1]; # the fixture with comments
ok my $comments = $article->comments;
is @{ $comments }, 8, 'loaded 8 comments';

is_deeply [ map { $_->id } @{$comments} ], [
    '19700101010000/1',     '19700101010000/1/1',
    '19700101010000/1/2',   '19700101010000/2',
    '19700101010000/3',     '19700101010000/3/1',
    '19700101010000/3/1/1', '19700101010000/3/1/2',
], 'Comments have the ids in the correct order';

for (@{ $comments }) {
    is $_->article->id, $article->id, "The comment knows it's article";
    ok !$_->{content}, 'Loading and getting comment id did not load content';
}

is_deeply [ map { $_->_epoch } @{$comments} ],
    [ 70, 71, 72, 80, 90, 91, 86491, 172891, ],
    'Comments have the expected epoch';

ok($_->{content}, 'Getting comment epoch did load content')
    for @{ $comments };

done_testing();
