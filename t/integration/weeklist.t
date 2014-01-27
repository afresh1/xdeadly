#!/usr/bin/perl
use Mojo::Base -strict;
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
    $article->date( scalar gmtime($epoch) );
    $article->save;

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
