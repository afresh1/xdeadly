#!/usr/bin/perl
use Mojo::Base -strict;
use Test::More;

use File::Temp qw/ tempdir /;

use XDeadly::Post;

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
is $loaded->path, "$dir/loaded/post";
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

done_testing();
