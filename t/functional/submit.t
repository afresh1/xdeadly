#!/usr/bin/perl
use Mojo::Base -strict;
use Test::Mojo;
use Test::More;

use File::Temp qw/ tempdir /;

use FindBin;
## no critic RequireBarewordIncludes
require "$FindBin::Bin/../../xdeadly";

my $dir = tempdir( CLEANUP => 1 );

my $t = Test::Mojo->new;
$t->app->config( data_dir => $dir );

$t->get_ok('/submit')->status_is(200);

my $csrf_token = $t->tx->res->dom->at("*[name='csrf_token']")->attr('value');

my %formdata = (
    name    => 'Testy McTesterson',
    email   => 'test@example.com',
    href    => 'example.com',
    subject => 'This is only a test',
    body    => "If this had been an actual post you would have been asked to evacuate.",

    csrf_token => $csrf_token,
);

my %expect = (
    sid          => undef,
    name         => undef,
    email        => undef,
    href         => undef,
    subject      => undef,
    body         => '',
    content_type => 'plain',
);
value_ok($_, $expect{$_}) for sort keys %expect;

$t->post_ok( '/submit', form => { %formdata, preview => 'Preview' } )
    ->status_is(200);

%expect = (%expect, %formdata);
value_ok($_, $expect{$_}) for sort keys %expect;

sub value_ok {
    my ($name, $expect) = @_;
    local $Test::Builder::Level = $Test::Builder::Level + 1;

    my $dom = $t->tx->res->dom;

    ok my $input = $dom->at("*[name='$name']"), "Found [$name]";
    return unless $input;

    my $value
        = $input->tag eq 'textarea' ? $input->text : $input->attr('value');
    is $value, $expect, "Correct value for [$name]";
}

done_testing();
