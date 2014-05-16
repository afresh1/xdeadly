#!/usr/bin/perl
use Mojo::Base -strict;
use Test::More;
use File::Temp qw/ tempdir /;

use lib 't/lib';
use XDeadlyFixtures;

use XDeadly::Article;

my $article = XDeadly::Article->new;

ok $article->is_article, 'New article is an article';
is $article->filename, 'article', 'New article filename is correct';

ok $article->id =~ /^\d{14}$/, 'New article generated id is 14 digits';

is_deeply $article->comments, [], 'an article without a dir has no comments';

my $dir = tempdir( CLEANUP => 1 );
XDeadlyFixtures::copy_good_data_fixtures($dir);

$article->data_dir( $dir );
is_deeply $article->comments, [], 'an unsaved article has no comments';

my $id = '19700101030000';
ok $article = XDeadly::Article->new( data_dir => $dir, id => $id ),
    'Load an article';

is $article->id, $id, 'Article has the correct id';

is $article->more_filename, 'article.more', 'more_filename is correct';
is $article->more_path, "$dir/$id/article.more", 'more_path is correct';

ok !$article->has_more, 'Article has no more';

open my $fh, '>', $article->more_path or die $!;
print $fh "But wait, there's more\n";
close $fh;

ok $article->has_more, 'Article now has more';
is $article->more, "But wait, there's more\n", 'loaded the correct more';

ok !$article->{content}, 'Still have not loaded article off disk';

ok my $body = $article->body, 'loaded the article';
ok $body =~ /ipsum/, 'Body looks right';
is length($body), 1343, 'Loaded the right length of body';


my $new_more = "It slices, it dices, it make juilienne fries!\n";
ok $article->more($new_more), 'set more';
is $article->more, $new_more, 'And it took';

ok $article->save, 'Saved the article';

ok $article = XDeadly::Article->new( data_dir => $dir, id => $id ),
    'reload the article';

ok $article->has_more, 'It knows it has more';
is $article->more, $new_more, 'And it is what we set it to';

ok !$article->{content}, 'Loading more does not load content';

$article->more(undef);
ok !$article->has_more, 'Can remove article more';
ok !$article->more, 'After removing, more does not exist';

$article->save;

ok $article = XDeadly::Article->new( data_dir => $dir, id => $id ),
    'reload the article';

ok !$article->has_more, 'After reloading, still have no more';
ok !$article->more, 'After reloading, still have more is empty';

my %attributes = (
    department => 'test department',
    topic      => 'topictest',
    topicimg   => 'topictest.jpg',
    topicquery => 'topictest',
);

while ( my ( $method, $value ) = each %attributes ) {
    ok $article = XDeadly::Article->new( data_dir => $dir, id => $id ),
        'reload the article';
    is $article->$method, $value, "Article has correct $method";
    ok $article->{content}, "$method causes the content to load";
}

done_testing;
