#!/usr/bin/perl
use Mojo::Base -strict;
use Test::Mojo;
use Test::More;

use XDeadly::Post;

use FindBin;
## no critic RequireBarewordIncludes
require "$FindBin::Bin/../../xdeadly";

my $t = Test::Mojo->new;

my $current = XDeadly::Post->new( id => 'current' );
my $base    = XDeadly::Post->new( id => 'base' );

$t->app->routes->get( '/do_indent' => sub {
    my ($self) = @_;
    $self->render( text => $self->_do_indent( 'Xyz', $current, $base ) );
});

$t->app->routes->get( '/indent' => sub {
    my ($self) = @_;
    $self->render( text => $self->indent( $current, $base ) );
});

$t->app->routes->get( '/unindent' => sub {
    my ($self) = @_;
    $self->render( text => $self->unindent( $current, $base ) );
});

foreach my $cl ( 0 .. 5 ) {
    $current->level($cl);

    my $real_base = $base;
    $base = undef;

    is $current->level, $cl, "Current level: $cl";
    is $t->app->level_diff($current), $cl, "level_diff = $cl";

    $t->get_ok('/do_indent')->content_is( 'Xyz' x $cl, "_do_indent $cl" );
    $t->get_ok('/indent')->content_is( '<ul>' x $cl, "indent $cl" );
    $t->get_ok('/unindent')->content_is( '</ul>' x $cl, "unindent $cl" );

    $t->get_ok('/do_indent?thread_mode=flat')
        ->content_is( '' x $cl, "_do_indent $cl [flat]" );
    $t->get_ok('/indent?thread_mode=flat')
        ->content_is( '' x $cl, "indent $cl [flat]" );
    $t->get_ok('/unindent?thread_mode=flat')
        ->content_is( '' x $cl, "unindent $cl [flat]" );

    $base = $real_base;
    foreach my $bl ( 0 .. 5 ) {
        $base->level($bl);

        is $base->level, $bl, "Base level: $bl";

        my $level_diff = $cl - $bl;
        $level_diff = 0 if $level_diff < 0;

        is $t->app->level_diff( $current, $base ), $level_diff,
            "level_diff = $cl - $bl == $level_diff";

        $t->get_ok('/do_indent')->content_is( 'Xyz' x $level_diff,
            "_do_indent $cl - $bl = $level_diff" );
        $t->get_ok('/indent')->content_is( '<ul>' x $level_diff,
            "indent $cl - $bl = $level_diff" );
        $t->get_ok('/unindent')->content_is( '</ul>' x $level_diff,
            "unindent $cl - $bl = $level_diff" );

        $t->get_ok('/do_indent?thread_mode=flat')
            ->content_is( '' x $level_diff,
            "_do_indent $cl - $bl = $level_diff [flat]" );
        $t->get_ok('/indent?thread_mode=flat')->content_is( '' x $level_diff,
            "indent $cl - $bl = $level_diff [flat]" );
        $t->get_ok('/unindent?thread_mode=flat')
            ->content_is( '' x $level_diff,
            "unindent $cl - $bl = $level_diff [flat]" );
    }
}

done_testing;
