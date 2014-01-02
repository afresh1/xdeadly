#!/usr/bin/perl
use Mojo::Base -strict;
use Test::More;
use File::Temp qw/ tempdir /;

use XDeadly::Article;
use XDeadly::Comment;

my $dir = '/nonexistent';
my $id  = '12345';

my $comment = XDeadly::Comment->new;

is $comment->filename, 'comment', 'A new comment has the right filename';

is $comment->id,     undef, 'A new comment has no id';
is $comment->parent, undef, 'A new comment has no parent';
is $comment->cid,    undef, 'A comment without a parent has no cid';

my %parents = (
    id  => XDeadly::Article->new( data_dir => $dir, id => 11111 ),
    cid => XDeadly::Article->new( data_dir => $dir, id => 22222 ),
);
my @tests  = (
    {   id    => "/1",
        cid   => 1,
        level => 1,
    },
    {   id    => "/2",
        cid   => 2,
        level => 1,
        make_parent => 1,
    },
    {   id    => "/2/1",
        cid   => 1,
        level => 2,
    },
    {   id    => "/2/2",
        cid   => 2,
        level => 2,
        make_parent => 1,
    },
    {   id    => "/2/2/1",
        cid   => 1,
        level => 3,
    },
);

foreach my $t (@tests) {
    foreach my $idtype (qw( id cid )) {
        my %t = %{ $t };

        my $parent = $parents{$idtype};
        my $article = $parent->is_article ? $parent : $parent->article;

        my $id = $t{id} = $article->id . $t{id};

        # set the id because otherwise we hit the filesystem.
        ok $comment = XDeadly::Comment->new(
            parent  => $parent,
            $idtype => $t{$idtype}
        ), "[$id][$idtype] New";

        # Have to set it in ->new, not with either of these methods
        # becase Mojo::Base seems to call the instantiator method
        # even if we are setting it.
        #$comment->$idtype( $t{$idtype} );
        #$comment->{$idtype} = $t{$idtype};

        $parents{$idtype} = $comment if delete $t{make_parent};

        is $comment->data_dir, $dir,     "[$id][$idtype] correct data_dir";
        is $comment->article,  $article, "[$id][$idtype] correct article";
        is $comment->parent,   $parent,  "[$id][$idtype] correct parent";

        foreach my $method ( sort keys %t ) {
            is $comment->$method, $t{$method},
                "[$id][$idtype] correct $method [$t{$method}]";
        }
    }
}

done_testing;
