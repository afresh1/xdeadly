#!/usr/bin/perl
use Mojo::Base -strict;
use Test::More;
use File::Temp qw/ tempdir /;

BEGIN {
    eval 'use XDeadly::Passwd';
    plan skip_all => 'Waiting for someone to write this' if $@;
}

use lib 't/lib';
use XDeadlyFixtures;

my $dir = tempdir( CLEANUP => 1 );
XDeadlyFixtures::copy_good_auth_fixtures($dir);

my $passwd = XDeadly::Passwd->new( auth_dir => $dir );

is $passwd->default_salt, '12345', 'Loaded the default salt';

ok $passwd->_check_sha1(
    test1 => '4d4b06ffce9d901d80fb4730160546ce1f2f400d'
), 'sha1 check uses the default salt';

ok $passwd->check( adminuser => 'Password1' ), 'Validate Admin Users password';
ok $passwd->check( test1 => 'test1' ), 'Validate test1 password';
ok $passwd->check( test2 => 'test2' ), 'Validate test2 password';

ok !$passwd->check( test1 => 'NotTheMama' ), 'Wrong password returns false';

is_deeply $passwd->check( test1 => 'test1' ), {
    id    => 'test1',
    name  => 'Testy McTesterson',
    email => 'test1@example.com',
    href  => undef,
    group => 'user',
    hash  => '4d4b06ffce9d901d80fb4730160546ce1f2f400d',
}, 'Right password returns user details';

is_deeply $passwd->users->{adminuser}, {
    id    => 'adminuser',
    name  => 'Admin User',
    email => 'admin@example.com',
    href  => 'example.com',
    group => 'admin',
    hash  => 'a147dd05552ba254df96ec51ce36e75fceafd093',
}, 'Can pull up user details directly';


my $test3 = {
    id    => 'test3',
    name  => 'Testy McTesterson III',
    email => 'test3@example.com',
    href  => 'test3.example.net',
    group => 'user',
    password => 'test3',
};

ok $passwd->add( $test3 ), 'Add a new user';
my $pass = delete $test3->{password};
$test3->{hash} = '6a9ae1eae3ca7d65a00d08a5cd9458bf7976fb64';

is_deeply $passwd->check( test3 => $pass ), $test3, 'Can validate new user';

# reload to make sure it saved
$passwd = XDeadly::Passwd->new( auth_dir => $dir );

is_deeply $passwd->check( test3 => $pass ), $test3, 'Can validate new user';

ok $passwd->disable( 'test3' ), 'Disabled test3';

ok !$passwd->check( test3 => $pass ), 'No longer can validate test3';

# Might want to do this differently
is $passwd->users->{test3}->{hash}, '*', 'Disabled by setting hash to *';

# reload to make sure it saved
$passwd = XDeadly::Passwd->new( auth_dir => $dir );

ok !$passwd->check( test3 => $pass ), 'Still cannot validate test3';

ok $passwd->passwd( test3 => $pass ), 'Change the password';

is_deeply $passwd->check( test3 => $pass ), $test3,
    'New password lets the user login';

ok !$passwd->disable( 'does_not_exist' ), 'fails to disable a non-user';
