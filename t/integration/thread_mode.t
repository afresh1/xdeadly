#!/usr/bin/perl
use Mojo::Base -strict;
use Test::Mojo;
use Test::More;

use XDeadly::Post;

use FindBin;
## no critic RequireBarewordIncludes
require "$FindBin::Bin/../../xdeadly";

my $t = Test::Mojo->new;
$t->app->routes->get( '/thread_mode' => sub {
    my ($self) = @_;
    $self->render( text => $self->thread_mode );
} );

my %expect = ( #[ things to try ]
    ''       => [ '', qw(collapsed unknown other special) ],
    expanded => ['expanded'],
    flat     => ['flat'],
);

foreach my $expect ( sort keys %expect ) {
    foreach my $try ( @{ $expect{$expect} } ) {
        $t->get_ok( '/thread_mode' . ( $try ? "?thread_mode=$try" : '' ) );
        $t->content_is( $expect, "[$try] thread_mode=[$expect]" );
    }
}

done_testing;
