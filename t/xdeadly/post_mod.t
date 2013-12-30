#!/usr/bin/perl
use Mojo::Base -strict;
use Test::More;

use File::Temp qw/ tempdir /;

use XDeadly::Post;
use XDeadly::PostMod;

my $dir = tempdir( CLEANUP => 1 );

is(XDeadly::PostMod->new->_points, undef,
    'a mod without a post has no points');

my $mod = XDeadly::PostMod->new(
    post => XDeadly::Post->new( data_dir => $dir, id => 'test' )->save );

is_deeply $mod->_points, { sum => 0, count => 0, votes => {} },
    'a new mod has no votes';

ok $mod->vote( test1 => -1 ), 'can cast a negative vote';
is $mod->score, -1, 'score is now -1';
is $mod->votes, 1, 'have 1 vote';

ok $mod->vote( test2 => 1 ), 'can cast a positive vote';
is $mod->score, 0, 'score is now 0';
is $mod->votes, 2, 'have 2 votes';

ok $mod->vote( test3 => 0 ), 'can cast a neutral vote';
is $mod->score, 0, 'score is now 0';
is $mod->votes, 2, 'still have 2 votes';

is_deeply $mod->_points, { sum => 0, count => 2, votes => {
    test1 => -1,
    test2 => 1,
    test3 => 0,
} }, 'Correct points for the vote. Neutral votes do not count'; 

ok $mod->has_voted( 'test1' ), 'test1 has voted';
ok $mod->has_voted( 'test2' ), 'test2 has voted';
ok $mod->has_voted( 'test3' ), 'test3 has voted';
ok !$mod->has_voted( 'test4' ), 'test4 has NOT voted';


$mod = XDeadly::PostMod->new(
    post => XDeadly::Post->new( data_dir => $dir, id => 'test' ) );

is_deeply $mod->_points, { sum => 0, count => 2, votes => {
    test1 => -1,
    test2 => 1,
    test3 => 0,
} }, 'Correct points After the reload. Neutral votes do not count'; 

done_testing;
