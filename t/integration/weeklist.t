#!/usr/bin/perl
use Mojo::Base -strict;
use Mojo::Util qw/ trim /;
use Test::Mojo;
use Test::More;
use File::Temp qw/ tempdir /;
use Time::Local qw/ timelocal /;

use FindBin;
## no critic RequireBarewordIncludes
require "$FindBin::Bin/../../xdeadly";

## no critic RequireBarewordIncludes
use lib "$FindBin::Bin/../lib";
use XDeadlyFixtures;

use XDeadly::Article;
use XDeadly::Comment;
my $dir = tempdir( CLEANUP => 1 );

my $t = Test::Mojo->new;
$t->app->config( data_dir => $dir );

$t->app->routes->get( '/weeklist' => sub {
    my ($self) = @_;
    $self->render( json => {
        articles  => [ $self->weeklist ],
        today     => $self->stash->{today},
        yesterday => $self->stash->{yesterday},
    } );
} );

$t->app->routes->get(
    '/olderstuff' => sub { shift->render('front_box_olderstuff') } );

my @epochs;
foreach my $mon ( 0 .. 5 ) {
    foreach my $mday ( 1 .. 3 ) {
        foreach my $hour ( 0 .. 3 ) {
            push @epochs, timelocal( 0, 0, $hour, $mday, $mon, 1970 );
        }
    }
}

my @articles;
foreach my $epoch ( sort { $b <=> $a } @epochs ) {
    my $id = XDeadly::Article::_epoch_to_id($epoch);
    my $article = XDeadly::Article->new( data_dir => $dir, id => $id );
    $article->subject( "Article [$id]" );
    $article->date( scalar gmtime($epoch) );
    $article->save;

    my $parent = $article;
    $parent = XDeadly::Comment->new( parent => $parent )->save for @articles;

    push @articles, $article;
}

is_deeply
    [ map { $_->id } $t->app->weeklist ],
    [ map { $_->id } @articles[ 5 .. 23 ] ],
    'Expected default weeklist';

sub expected_weeklist {
    my ( $date, $expect ) = @_;
    my $query = $date ? "?date=$date" : '';
    $date ||= 'default date';

    my @articles = @{ $expect->{articles} };
    $expect->{articles} = [ map { $_->id } @articles ];

    $t->get_ok("/weeklist$query");
    is_deeply $t->tx->res->json, $expect, "Expected weeklist for $date";

    $t->get_ok("/olderstuff$query");
    my $dom = $t->tx->res->dom;

    ok my $yesterdays_link
        = $dom->at("a[href^=/?date=$expect->{yesterday}]");
    is $yesterdays_link->all_text, "Yesterday's Edition...",
        'Correct link to yesterday';

    my $table = $dom->at('table table');
    is trim( $table->at('tr')->at('td')->all_text ), 'Older Stuff',
        'Got table with Older Stuff';

    my $trs = $table->at('tr:nth-of-type(2)')->at('table')->find('tr');

    my $j = 0;
    my $last_shortdate = '';
    for (my $i = 0 ; $i <= $#articles ; $i++) {
        my $article = $articles[$i];
        my $tr = $trs->[$j++];

        if ($last_shortdate ne $article->short_date) {
            $last_shortdate = $article->short_date;
            is trim( $tr->all_text ), $article->short_date,
                "Found short_date header for $last_shortdate";
            $tr = $trs->[$j++];
        }

        is trim( $tr->at('td')->all_text ), $article->time,
            "[$date][$article] have time";

        ok my $link
            = $tr->at('td:nth-of-type(2)')->at("a[href^=/article/$article]");
        is trim( $link->text ), "Article [$article]",
            "[$date][$article] link to the article";

        my $comments = @{ $article->comments };
        like trim( $tr->at('td:nth-of-type(2)')->all_text ),
	     qr/ \($comments\)$/,
            "[$date][$article] Correct number of comments";
    }
}

sub epoch_to_date {
    my ($epoch) = @_;
    my ( $year, $mon, $mday ) = ( gmtime($epoch) )[ 5, 4, 3 ];
    $year += 1900;
    $mon++;
    return sprintf '%04d%02d%02d', $year, $mon, $mday;
}

expected_weeklist(
    '' => {
        articles  => [ @articles[ 5 .. 23 ] ],
        today     => epoch_to_date(time),
        yesterday => epoch_to_date( time - 86400 ),
    }
);
expected_weeklist(
    19700502 => {
        articles  => [ @articles[ 21 .. 39 ] ],
        today     => '19700502',
        yesterday => '19700501'
    }
);
expected_weeklist(
    19700501 => {
        articles  => [ @articles[ 25 .. 43 ] ],
        today     => '19700501',
        yesterday => '19700430'
    }
);
expected_weeklist(
    19700403 => {
        articles  => [ @articles[ 29 .. 47 ] ],
        today     => '19700403',
        yesterday => '19700402'
    }
);

done_testing;
