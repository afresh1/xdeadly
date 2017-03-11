#!/usr/bin/perl
use Mojo::Base -strict;
use Test::Mojo;
use Test::More;

use FindBin;
## no critic RequireBarewordIncludes
require "$FindBin::Bin/../../xdeadly";

my $t = Test::Mojo->new;
$t->get_ok('/login')->element_exists('input[name="username"]')
    ->element_exists('input[name="password"]')
    ->element_exists_not('form[action="/login"][class*="form-with-error"]')
    ->element_exists_not('input[name="username"][class*="field-with-error"]')
    ->element_exists_not('input[name="password"][class*="field-with-error"]')
    ->element_exists_not('a[href="/submissions"]')
    ->element_exists_not('a[href="/prefs"]')
    ->element_exists_not('a[href="/deauth"]')
    ->content_unlike(qr/\bUnknown User\b/);

my $csrf_token = $t->tx->res->dom->at('input[name="csrf_token"]')->val;
my %submit = ( submit => "Submit", csrf_token => $csrf_token );

$t->post_ok( '/login' => form => \%submit )
    ->element_exists('form[action="/login"][class*="form-with-error"]')
    ->element_exists('input[name="username"][class*="field-with-error"]')
    ->element_exists('input[name="password"][class*="field-with-error"]')
    ->content_like(qr/\bUnknown User\b/);

$t->post_ok(
    '/login' => form => { username => "foo", password => "bar", %submit } )
    ->element_exists('form[action="/login"][class*="form-with-error"]')
    ->element_exists_not('input[name="username"][class*="field-with-error"]')
    ->element_exists_not('input[name="password"][class*="field-with-error"]')
    ->content_like(qr/\bUnknown User\b/);

$t->post_ok( '/login' => form =>
        { username => "user", password => "password", %submit } )
->status_is(302);
$t->get_ok( $t->tx->res->headers->location )
    #->content_like(qr/\bThanks\b/)
    ->element_exists_not('a[href="/login"]')
    ->element_exists_not('a[href="/submissions"]')
    ->element_exists('a[href="/prefs"]')
    ->element_exists('a[href="/deauth"]');

$t->get_ok('/login');
$t->post_ok( '/login' => form =>
        { username => "editor", password => "password", %submit } )
->status_is(302);
$t->get_ok( $t->tx->res->headers->location )
    #->content_like(qr/\bThanks\b/)
    ->element_exists_not('a[href="/login"]')
    ->element_exists('a[href="/submissions"]')
    ->element_exists('a[href="/prefs"]')
    ->element_exists('a[href="/deauth"]');

done_testing;
