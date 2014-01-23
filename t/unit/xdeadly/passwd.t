#!/usr/bin/perl
use Mojo::Base -strict;
use Test::More;

BEGIN {
    eval 'use XDeadly::Passwd';
    plan skip_all => 'Waiting for someone to write this' if $@;
}

my $passwd = XDeadly::Passwd->new(
    default_salt => 12345,
);

$passwd->default_salt, '12345', 'Loaded the default salt';

ok $passwd->_check_sha1(
    test1 => '4d4b06ffce9d901d80fb4730160546ce1f2f400d'
), 'sha1 check uses the default salt';

done_testing;
