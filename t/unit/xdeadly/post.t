#!/usr/bin/perl
use Mojo::Base -strict;
use Test::More;

use File::Temp qw/ tempdir /;
use File::Path qw/ make_path /;
use File::Spec;

use XDeadly::Post;

my $dir = '/nonexistent';

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

my $post = XDeadly::Post->new;
ok defined $post, 'create a new post which is defined';
is $post->data_dir, undef, 'new post has no data_dir';
is $post->id, undef, 'new post has no id';

is $post->level, 0, 'Posts have a level of 0';

ok $post = XDeadly::Post->new( data_dir => $dir, id => 'test' );
is $post->data_dir, $dir, 'Got a data_dir';
is $post->id, 'test', 'Got an id';
is $post->dir,   "$dir/test";
is $post->path,  "$dir/test/post";
is $post->level, 0, 'Posts have a level of 0';
is "$post", 'test', 'Post stringifies to the id';
ok $post->has_path, 'loaded post has a path';
ok !$post->is_article, 'loaded post is not an article';
is $post->filename, 'post', 'loaded post filename is post';


$post = XDeadly::Post->new;
ok !$post->parse($message_text), 'Created a test post that is false';
is $post->id, undef, 'parsed post has no id';
ok defined $post, 'parsed post is defined';
ok $post->body, 'but it has a body';

is $post->_epoch, 1377010297, 'correctly parsed the epoch';

is $post->build_body, $body, 'Body matches';
is $post->headers->header($_), $headers{$_}, "Header($_) matches"
    for keys %headers;

is $post->subject, $headers{Subject}, 'Subject shortcut works';
is $post->name,    $headers{Name},    'Name shortcut works';
is $post->email,   $headers{EMail},   'EMail shortcut works';
is $post->href,    $headers{HREF},    'HREF shortcut works';
is $post->host,    $headers{Host},    'Host shortcut works';
is $post->date,    $headers{Date}.' (GMT)', 'Date shortcut works';

eval { XDeadly::Post::_parse_ctime('Thu Jan 01 00:00:00 UTC 1970') };
ok $@ =~ /Invalid date/, 'Date with a timezone is invalid';

ok $post->mod->isa( 'XDeadly::PostMod' ), 'We have a mod that we can vote with';

done_testing();
