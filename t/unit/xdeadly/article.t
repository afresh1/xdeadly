#!/usr/bin/perl
use Mojo::Base -strict;
use Test::More;

use XDeadly::Article;

my $message_text = q{name: Testy McTesterson
email: test@example.com
href: example.com
date: Tue Aug 20 14:51:37 2013
host: 127.0.0.1
subject: This is only a test
dept: who knows what
topic: ttt-topic

If this had been an actual post you would have been asked to evacuate.
};

my %headers = (
    Name    => 'Testy McTesterson',
    EMail   => 'test@example.com',
    HREF    => 'example.com',
    Date    => 'Tue Aug 20 14:51:37 2013',
    Host    => '127.0.0.1',
    Subject => 'This is only a test',
    Dept    => 'who knows what',
    Topic   => 'ttt-topic',
);
my $body
    = "If this had been an actual post you would have been asked to evacuate.\x0a";

my %shortcuts = (
    department => 'Dept',
    topic      => 'Topic',
);

my $dir = '/nonexistent';
my $id  = '12345';

my $article = XDeadly::Article->new;

ok $article->is_article, 'New article is an article';
is $article->filename, 'article', 'New article filename is correct';

ok $article->id =~ /^\d{14}$/, 'New article generated id is 14 digits';

ok $article = XDeadly::Article->new( data_dir => $dir, id => $id ),
    'article with all needed arguments';

is $article->id, $id, 'Article has the correct id';

is $article->more_filename, 'article.more', 'more_filename is correct';
is $article->more_path, "$dir/$id/article.more", 'more_path is correct';

ok !$article->has_more, 'New article has no more';

ok $article->more("But wait, there's more\n"), 'Can set more';

ok $article->has_more, 'Article now has more';
is $article->more, "But wait, there's more\n", 'correct more';

$article->more(undef);
ok !$article->has_more, 'Can remove article more';
ok !$article->more,     'After removing, more does not exist';

ok !$article->body, 'Article has no body';
ok $article->body( $body ), 'Set article body';
is $article->body, $body, 'Article body is correct';

ok $article = XDeadly::Article->new( data_dir => $dir, id => $id ),
    'article with all needed arguments';

ok !$article->topicimg,   'New article has no img';
ok !$article->topicquery, 'New article has no topicquery';
foreach my $method ( keys %shortcuts ) {
    ok !$article->$method, "New article has no $method";

    my $value = $headers{ $shortcuts{$method} };
    ok $article->$method($value), 'Can try to set the method';
    is $article->$method, $value, "Correctly load the value";
}
is $article->topicimg, "$headers{Topic}.jpg", 'article now has img';
is $article->topicquery, $headers{Topic}, 'New article has no topicquery';

ok $article->topicquery("$headers{Topic} Plus"), 'Set topicquery';
is $article->topic, "$headers{Topic} Plus", 'topicquery adjusts topic';
is $article->topicimg, "$headers{Topic} Plus.jpg",
    'topicquery adjusts topicimg';


ok $article = XDeadly::Article->new( data_dir => $dir, id => $id ),
    'article with all needed arguments';

foreach my $header ( keys %headers ) {
    ok !$article->headers->header($header),
        "New article has no $header header";

    my $value = $headers{$header};

    ok $article->headers->header( $header => $value ),
        "Can set the $header header";

    is $article->headers->header($header), $value, "Set $header header";
}

foreach my $method ( keys %shortcuts ) {
    my $header = $shortcuts{$method};
    my $value  = $headers{$header};
    is $article->$method, $value, "Shortcut $method got value from $header";
}

ok $article = XDeadly::Article->new( data_dir => $dir, id => $id ),
    'article with all needed arguments';

ok $article->parse( $message_text ), 'Parse a message';

foreach my $header ( keys %headers ) {
    my $value = $headers{$header};
    is $article->headers->header($header), $value, "Set $header header";
}

foreach my $method ( keys %shortcuts ) {
    my $header = $shortcuts{$method};
    my $value  = $headers{$header};
    is $article->$method, $value, "Shortcut $method got value from $header";
}

done_testing;
