#!/usr/bin/perl
use Mojo::Base -strict;
use Test::Mojo;
use Test::More;
use File::Temp qw/ tempdir /;

use FindBin;
## no critic RequireBarewordIncludes
require "$FindBin::Bin/../../xdeadly";

## no critic RequireBarewordIncludes
use lib "$FindBin::Bin/../lib";
use XDeadlyFixtures;

use XDeadly::Comment;
my $dir = tempdir( CLEANUP => 1 );
XDeadlyFixtures::copy_good_data_fixtures($dir);

my $t = Test::Mojo->new;
$t->app->config( data_dir => $dir );

my @ids = (
    '19700101010000',     '19700101010000/3',
    '19700101010000/3/1', '19700101010000/3/1/1',
    '19700101020000',     '19700101030000',
);
my @cids  = ('12345/6/7/8');
my @modes = qw( flat collapsed expanded );

$t->app->hook( around_action => sub {
    my ( $next, $c, $action, $last ) = @_;

    my $url = $c->url_with;

    unless ($last) {
        $c->req->url->query->remove('id');
        $c->req->url->query->remove('cid');

        check_links( $c, $url );
    }

    return $next->();
} );

my %links = (
    '/?id=19700101010000' => [
        {   'href'  => '/article/19700101010000',
            'title' => '8 comments Xd ago'
        },
        {   'href'  => '/article/19700101010000?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101010000?mode=expanded',
            'title' => 'Expanded'
        }
    ],
    '/?id=19700101010000&cid=12345/6/7/8' => [
        {   'href'  => '/article/12345/6/7/8',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8',
            'title' => 'Reply to this comment'
        }
    ],
    '/?id=19700101010000&cid=12345/6/7/8&mode=collapsed' => [
        {   'href'  => '/article/12345/6/7/8?mode=collapsed',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8?mode=collapsed',
            'title' => 'Reply to this comment'
        }
    ],
    '/?id=19700101010000&cid=12345/6/7/8&mode=expanded' => [
        {   'href'  => '/article/12345/6/7/8?mode=expanded',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8?mode=expanded',
            'title' => 'Reply to this comment'
        }
    ],
    '/?id=19700101010000&cid=12345/6/7/8&mode=flat' => [
        {   'href'  => '/article/12345/6/7/8?mode=flat',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8?mode=flat',
            'title' => 'Reply to this comment'
        }
    ],
    '/?id=19700101010000&mode=collapsed' => [
        {   'href'  => '/article/19700101010000?mode=collapsed',
            'title' => '8 comments Xd ago'
        },
        {   'href'  => '/article/19700101010000?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101010000?mode=expanded',
            'title' => 'Expanded'
        }
    ],
    '/?id=19700101010000&mode=expanded' => [
        {   'href'  => '/article/19700101010000?mode=expanded',
            'title' => '8 comments Xd ago'
        },
        {   'href'  => '/article/19700101010000?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101010000',
            'title' => 'Collapsed'
        }
    ],
    '/?id=19700101010000&mode=flat' => [
        {   'href'  => '/article/19700101010000?mode=flat',
            'title' => '8 comments Xd ago'
        },
        {   'href'  => '/article/19700101010000?mode=expanded',
            'title' => 'Expanded'
        },
        {   'href'  => '/article/19700101010000',
            'title' => 'Collapsed'
        }
    ],
    '/?id=19700101010000/3' => [
        {   'href'  => '/article/19700101010000/3',
            'title' => '3 comments Xd ago'
        },
        {   'href'  => '/article/19700101010000',
            'title' => 'Up: Test Post #1'
        },
        {   'href'  => '/article/19700101010000/3?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101010000/3?mode=expanded',
            'title' => 'Expanded'
        }
    ],
    '/?id=19700101010000/3&cid=12345/6/7/8' => [
        {   'href'  => '/article/12345/6/7/8',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8',
            'title' => 'Reply to this comment'
        }
    ],
    '/?id=19700101010000/3&cid=12345/6/7/8&mode=collapsed' => [
        {   'href'  => '/article/12345/6/7/8?mode=collapsed',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8?mode=collapsed',
            'title' => 'Reply to this comment'
        }
    ],
    '/?id=19700101010000/3&cid=12345/6/7/8&mode=expanded' => [
        {   'href'  => '/article/12345/6/7/8?mode=expanded',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8?mode=expanded',
            'title' => 'Reply to this comment'
        }
    ],
    '/?id=19700101010000/3&cid=12345/6/7/8&mode=flat' => [
        {   'href'  => '/article/12345/6/7/8?mode=flat',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8?mode=flat',
            'title' => 'Reply to this comment'
        }
    ],
    '/?id=19700101010000/3&mode=collapsed' => [
        {   'href'  => '/article/19700101010000/3?mode=collapsed',
            'title' => '3 comments Xd ago'
        },
        {   'href'  => '/article/19700101010000?mode=collapsed',
            'title' => 'Up: Test Post #1'
        },
        {   'href'  => '/article/19700101010000/3?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101010000/3?mode=expanded',
            'title' => 'Expanded'
        }
    ],
    '/?id=19700101010000/3&mode=expanded' => [
        {   'href'  => '/article/19700101010000/3?mode=expanded',
            'title' => '3 comments Xd ago'
        },
        {   'href'  => '/article/19700101010000?mode=expanded',
            'title' => 'Up: Test Post #1'
        },
        {   'href'  => '/article/19700101010000/3?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101010000/3',
            'title' => 'Collapsed'
        }
    ],
    '/?id=19700101010000/3&mode=flat' => [
        {   'href'  => '/article/19700101010000/3?mode=flat',
            'title' => '3 comments Xd ago'
        },
        {   'href'  => '/article/19700101010000?mode=flat',
            'title' => 'Up: Test Post #1'
        },
        {   'href'  => '/article/19700101010000/3?mode=expanded',
            'title' => 'Expanded'
        },
        {   'href'  => '/article/19700101010000/3',
            'title' => 'Collapsed'
        }
    ],
    '/?id=19700101010000/3/1' => [
        {   'href'  => '/article/19700101010000/3/1',
            'title' => '2 comments Xd ago'
        },
        {   'href'  => '/article/19700101010000/3',
            'title' => 'Up: Test Comment #1_3'
        },
        {   'href'  => '/article/19700101010000/3/1?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101010000/3/1?mode=expanded',
            'title' => 'Expanded'
        }
    ],
    '/?id=19700101010000/3/1&cid=12345/6/7/8' => [
        {   'href'  => '/article/12345/6/7/8',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8',
            'title' => 'Reply to this comment'
        }
    ],
    '/?id=19700101010000/3/1&cid=12345/6/7/8&mode=collapsed' => [
        {   'href'  => '/article/12345/6/7/8?mode=collapsed',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8?mode=collapsed',
            'title' => 'Reply to this comment'
        }
    ],
    '/?id=19700101010000/3/1&cid=12345/6/7/8&mode=expanded' => [
        {   'href'  => '/article/12345/6/7/8?mode=expanded',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8?mode=expanded',
            'title' => 'Reply to this comment'
        }
    ],
    '/?id=19700101010000/3/1&cid=12345/6/7/8&mode=flat' => [
        {   'href'  => '/article/12345/6/7/8?mode=flat',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8?mode=flat',
            'title' => 'Reply to this comment'
        }
    ],
    '/?id=19700101010000/3/1&mode=collapsed' => [
        {   'href'  => '/article/19700101010000/3/1?mode=collapsed',
            'title' => '2 comments Xd ago'
        },
        {   'href'  => '/article/19700101010000/3?mode=collapsed',
            'title' => 'Up: Test Comment #1_3'
        },
        {   'href'  => '/article/19700101010000/3/1?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101010000/3/1?mode=expanded',
            'title' => 'Expanded'
        }
    ],
    '/?id=19700101010000/3/1&mode=expanded' => [
        {   'href'  => '/article/19700101010000/3/1?mode=expanded',
            'title' => '2 comments Xd ago'
        },
        {   'href'  => '/article/19700101010000/3?mode=expanded',
            'title' => 'Up: Test Comment #1_3'
        },
        {   'href'  => '/article/19700101010000/3/1?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101010000/3/1',
            'title' => 'Collapsed'
        }
    ],
    '/?id=19700101010000/3/1&mode=flat' => [
        {   'href'  => '/article/19700101010000/3/1?mode=flat',
            'title' => '2 comments Xd ago'
        },
        {   'href'  => '/article/19700101010000/3?mode=flat',
            'title' => 'Up: Test Comment #1_3'
        },
        {   'href'  => '/article/19700101010000/3/1?mode=expanded',
            'title' => 'Expanded'
        },
        {   'href'  => '/article/19700101010000/3/1',
            'title' => 'Collapsed'
        }
    ],
    '/?id=19700101010000/3/1/1' => [
        {   'href'  => '/article/19700101010000/3/1/1',
            'title' => '0 comments Xd ago'
        },
        {   'href'  => '/article/19700101010000/3/1',
            'title' => 'Up: Test Comment #1_3_1'
        },
        {   'href'  => '/article/19700101010000/3/1/1?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101010000/3/1/1?mode=expanded',
            'title' => 'Expanded'
        }
    ],
    '/?id=19700101010000/3/1/1&cid=12345/6/7/8' => [
        {   'href'  => '/article/12345/6/7/8',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8',
            'title' => 'Reply to this comment'
        }
    ],
    '/?id=19700101010000/3/1/1&cid=12345/6/7/8&mode=collapsed' => [
        {   'href'  => '/article/12345/6/7/8?mode=collapsed',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8?mode=collapsed',
            'title' => 'Reply to this comment'
        }
    ],
    '/?id=19700101010000/3/1/1&cid=12345/6/7/8&mode=expanded' => [
        {   'href'  => '/article/12345/6/7/8?mode=expanded',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8?mode=expanded',
            'title' => 'Reply to this comment'
        }
    ],
    '/?id=19700101010000/3/1/1&cid=12345/6/7/8&mode=flat' => [
        {   'href'  => '/article/12345/6/7/8?mode=flat',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8?mode=flat',
            'title' => 'Reply to this comment'
        }
    ],
    '/?id=19700101010000/3/1/1&mode=collapsed' => [
        {   'href'  => '/article/19700101010000/3/1/1?mode=collapsed',
            'title' => '0 comments Xd ago'
        },
        {   'href'  => '/article/19700101010000/3/1?mode=collapsed',
            'title' => 'Up: Test Comment #1_3_1'
        },
        {   'href'  => '/article/19700101010000/3/1/1?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101010000/3/1/1?mode=expanded',
            'title' => 'Expanded'
        }
    ],
    '/?id=19700101010000/3/1/1&mode=expanded' => [
        {   'href'  => '/article/19700101010000/3/1/1?mode=expanded',
            'title' => '0 comments Xd ago'
        },
        {   'href'  => '/article/19700101010000/3/1?mode=expanded',
            'title' => 'Up: Test Comment #1_3_1'
        },
        {   'href'  => '/article/19700101010000/3/1/1?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101010000/3/1/1',
            'title' => 'Collapsed'
        }
    ],
    '/?id=19700101010000/3/1/1&mode=flat' => [
        {   'href'  => '/article/19700101010000/3/1/1?mode=flat',
            'title' => '0 comments Xd ago'
        },
        {   'href'  => '/article/19700101010000/3/1?mode=flat',
            'title' => 'Up: Test Comment #1_3_1'
        },
        {   'href'  => '/article/19700101010000/3/1/1?mode=expanded',
            'title' => 'Expanded'
        },
        {   'href'  => '/article/19700101010000/3/1/1',
            'title' => 'Collapsed'
        }
    ],
    '/?id=19700101020000' => [
        {   'href'  => '/article/19700101020000',
            'title' => '0 comments Xd ago'
        },
        {   'href'  => '/article/19700101020000?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101020000?mode=expanded',
            'title' => 'Expanded'
        }
    ],
    '/?id=19700101020000&cid=12345/6/7/8' => [
        {   'href'  => '/article/12345/6/7/8',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8',
            'title' => 'Reply to this comment'
        }
    ],
    '/?id=19700101020000&cid=12345/6/7/8&mode=collapsed' => [
        {   'href'  => '/article/12345/6/7/8?mode=collapsed',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8?mode=collapsed',
            'title' => 'Reply to this comment'
        }
    ],
    '/?id=19700101020000&cid=12345/6/7/8&mode=expanded' => [
        {   'href'  => '/article/12345/6/7/8?mode=expanded',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8?mode=expanded',
            'title' => 'Reply to this comment'
        }
    ],
    '/?id=19700101020000&cid=12345/6/7/8&mode=flat' => [
        {   'href'  => '/article/12345/6/7/8?mode=flat',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8?mode=flat',
            'title' => 'Reply to this comment'
        }
    ],
    '/?id=19700101020000&mode=collapsed' => [
        {   'href'  => '/article/19700101020000?mode=collapsed',
            'title' => '0 comments Xd ago'
        },
        {   'href'  => '/article/19700101020000?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101020000?mode=expanded',
            'title' => 'Expanded'
        }
    ],
    '/?id=19700101020000&mode=expanded' => [
        {   'href'  => '/article/19700101020000?mode=expanded',
            'title' => '0 comments Xd ago'
        },
        {   'href'  => '/article/19700101020000?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101020000',
            'title' => 'Collapsed'
        }
    ],
    '/?id=19700101020000&mode=flat' => [
        {   'href'  => '/article/19700101020000?mode=flat',
            'title' => '0 comments Xd ago'
        },
        {   'href'  => '/article/19700101020000?mode=expanded',
            'title' => 'Expanded'
        },
        {   'href'  => '/article/19700101020000',
            'title' => 'Collapsed'
        }
    ],
    '/?id=19700101030000' => [
        {   'href'  => '/article/19700101030000',
            'title' => '0 comments Xd ago'
        },
        {   'href'  => '/article/19700101030000?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101030000?mode=expanded',
            'title' => 'Expanded'
        }
    ],
    '/?id=19700101030000&cid=12345/6/7/8' => [
        {   'href'  => '/article/12345/6/7/8',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8',
            'title' => 'Reply to this comment'
        }
    ],
    '/?id=19700101030000&cid=12345/6/7/8&mode=collapsed' => [
        {   'href'  => '/article/12345/6/7/8?mode=collapsed',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8?mode=collapsed',
            'title' => 'Reply to this comment'
        }
    ],
    '/?id=19700101030000&cid=12345/6/7/8&mode=expanded' => [
        {   'href'  => '/article/12345/6/7/8?mode=expanded',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8?mode=expanded',
            'title' => 'Reply to this comment'
        }
    ],
    '/?id=19700101030000&cid=12345/6/7/8&mode=flat' => [
        {   'href'  => '/article/12345/6/7/8?mode=flat',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8?mode=flat',
            'title' => 'Reply to this comment'
        }
    ],
    '/?id=19700101030000&mode=collapsed' => [
        {   'href'  => '/article/19700101030000?mode=collapsed',
            'title' => '0 comments Xd ago'
        },
        {   'href'  => '/article/19700101030000?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101030000?mode=expanded',
            'title' => 'Expanded'
        }
    ],
    '/?id=19700101030000&mode=expanded' => [
        {   'href'  => '/article/19700101030000?mode=expanded',
            'title' => '0 comments Xd ago'
        },
        {   'href'  => '/article/19700101030000?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101030000',
            'title' => 'Collapsed'
        }
    ],
    '/?id=19700101030000&mode=flat' => [
        {   'href'  => '/article/19700101030000?mode=flat',
            'title' => '0 comments Xd ago'
        },
        {   'href'  => '/article/19700101030000?mode=expanded',
            'title' => 'Expanded'
        },
        {   'href'  => '/article/19700101030000',
            'title' => 'Collapsed'
        }
    ],
    '/article/19700101010000' => [
        {   'href'  => '/article/19700101010000?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101010000?mode=expanded',
            'title' => 'Expanded'
        },
        {   'href'  => '/article/19700101020000',
            'title' => 'Test Post #2 >>'
        }
    ],
    '/article/19700101010000/3' => [
        {   'href'  => '/article/19700101010000/2',
            'title' => '<< Test Comment #2'
        },
        {   'href'  => '/article/19700101010000',
            'title' => 'Up: Test Post #1'
        },
        {   'href'  => '/article/19700101010000/3?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101010000/3?mode=expanded',
            'title' => 'Expanded'
        },
        {   'href'  => '/article/19700101010000/3/1',
            'title' => 'Test Comment #1_3_1 >>'
        }
    ],
    '/article/19700101010000/3/1' => [
        {   'href'  => '/article/19700101010000/3',
            'title' => '<< Test Comment #1_3'
        },
        {   'href'  => '/article/19700101010000/3',
            'title' => 'Up: Test Comment #1_3'
        },
        {   'href'  => '/article/19700101010000/3/1?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101010000/3/1?mode=expanded',
            'title' => 'Expanded'
        },
        {   'href'  => '/article/19700101010000/3/1/1',
            'title' => 'Test Comment #1_3_1_1 >>'
        }
    ],
    '/article/19700101010000/3/1/1' => [
        {   'href'  => '/article/19700101010000/3/1',
            'title' => '<< Test Comment #1_3_1'
        },
        {   'href'  => '/article/19700101010000/3/1',
            'title' => 'Up: Test Comment #1_3_1'
        },
        {   'href'  => '/article/19700101010000/3/1/1?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101010000/3/1/1?mode=expanded',
            'title' => 'Expanded'
        },
        {   'href'  => '/article/19700101010000/3/1/2',
            'title' => 'Test Post #1_3_1_2 >>'
        }
    ],
    '/article/19700101010000/3/1/1?cid=12345/6/7/8' => [
        {   'href'  => '/article/12345/6/7/8',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8',
            'title' => 'Reply to this comment'
        }
    ],
    '/article/19700101010000/3/1/1?cid=12345/6/7/8&mode=collapsed' => [
        {   'href'  => '/article/12345/6/7/8?mode=collapsed',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8?mode=collapsed',
            'title' => 'Reply to this comment'
        }
    ],
    '/article/19700101010000/3/1/1?cid=12345/6/7/8&mode=expanded' => [
        {   'href'  => '/article/12345/6/7/8?mode=expanded',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8?mode=expanded',
            'title' => 'Reply to this comment'
        }
    ],
    '/article/19700101010000/3/1/1?cid=12345/6/7/8&mode=flat' => [
        {   'href'  => '/article/12345/6/7/8?mode=flat',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8?mode=flat',
            'title' => 'Reply to this comment'
        }
    ],
    '/article/19700101010000/3/1/1?mode=collapsed' => [
        {   'href'  => '/article/19700101010000/3/1?mode=collapsed',
            'title' => '<< Test Comment #1_3_1'
        },
        {   'href'  => '/article/19700101010000/3/1?mode=collapsed',
            'title' => 'Up: Test Comment #1_3_1'
        },
        {   'href'  => '/article/19700101010000/3/1/1?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101010000/3/1/1?mode=expanded',
            'title' => 'Expanded'
        },
        {   'href'  => '/article/19700101010000/3/1/2?mode=collapsed',
            'title' => 'Test Post #1_3_1_2 >>'
        }
    ],
    '/article/19700101010000/3/1/1?mode=expanded' => [
        {   'href'  => '/article/19700101010000/3/1?mode=expanded',
            'title' => '<< Test Comment #1_3_1'
        },
        {   'href'  => '/article/19700101010000/3/1?mode=expanded',
            'title' => 'Up: Test Comment #1_3_1'
        },
        {   'href'  => '/article/19700101010000/3/1/1?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101010000/3/1/1',
            'title' => 'Collapsed'
        },
        {   'href'  => '/article/19700101010000/3/1/2?mode=expanded',
            'title' => 'Test Post #1_3_1_2 >>'
        }
    ],
    '/article/19700101010000/3/1/1?mode=flat' => [
        {   'href'  => '/article/19700101010000/3/1?mode=flat',
            'title' => '<< Test Comment #1_3_1'
        },
        {   'href'  => '/article/19700101010000/3/1?mode=flat',
            'title' => 'Up: Test Comment #1_3_1'
        },
        {   'href'  => '/article/19700101010000/3/1/1?mode=expanded',
            'title' => 'Expanded'
        },
        {   'href'  => '/article/19700101010000/3/1/1',
            'title' => 'Collapsed'
        },
        {   'href'  => '/article/19700101010000/3/1/2?mode=flat',
            'title' => 'Test Post #1_3_1_2 >>'
        }
    ],
    '/article/19700101010000/3/1?cid=12345/6/7/8' => [
        {   'href'  => '/article/12345/6/7/8',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8',
            'title' => 'Reply to this comment'
        }
    ],
    '/article/19700101010000/3/1?cid=12345/6/7/8&mode=collapsed' => [
        {   'href'  => '/article/12345/6/7/8?mode=collapsed',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8?mode=collapsed',
            'title' => 'Reply to this comment'
        }
    ],
    '/article/19700101010000/3/1?cid=12345/6/7/8&mode=expanded' => [
        {   'href'  => '/article/12345/6/7/8?mode=expanded',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8?mode=expanded',
            'title' => 'Reply to this comment'
        }
    ],
    '/article/19700101010000/3/1?cid=12345/6/7/8&mode=flat' => [
        {   'href'  => '/article/12345/6/7/8?mode=flat',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8?mode=flat',
            'title' => 'Reply to this comment'
        }
    ],
    '/article/19700101010000/3/1?mode=collapsed' => [
        {   'href'  => '/article/19700101010000/3?mode=collapsed',
            'title' => '<< Test Comment #1_3'
        },
        {   'href'  => '/article/19700101010000/3?mode=collapsed',
            'title' => 'Up: Test Comment #1_3'
        },
        {   'href'  => '/article/19700101010000/3/1?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101010000/3/1?mode=expanded',
            'title' => 'Expanded'
        },
        {   'href'  => '/article/19700101010000/3/1/1?mode=collapsed',
            'title' => 'Test Comment #1_3_1_1 >>'
        }
    ],
    '/article/19700101010000/3/1?mode=expanded' => [
        {   'href'  => '/article/19700101010000/3?mode=expanded',
            'title' => '<< Test Comment #1_3'
        },
        {   'href'  => '/article/19700101010000/3?mode=expanded',
            'title' => 'Up: Test Comment #1_3'
        },
        {   'href'  => '/article/19700101010000/3/1?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101010000/3/1',
            'title' => 'Collapsed'
        },
        {   'href'  => '/article/19700101010000/3/1/1?mode=expanded',
            'title' => 'Test Comment #1_3_1_1 >>'
        }
    ],
    '/article/19700101010000/3/1?mode=flat' => [
        {   'href'  => '/article/19700101010000/3?mode=flat',
            'title' => '<< Test Comment #1_3'
        },
        {   'href'  => '/article/19700101010000/3?mode=flat',
            'title' => 'Up: Test Comment #1_3'
        },
        {   'href'  => '/article/19700101010000/3/1?mode=expanded',
            'title' => 'Expanded'
        },
        {   'href'  => '/article/19700101010000/3/1',
            'title' => 'Collapsed'
        },
        {   'href'  => '/article/19700101010000/3/1/1?mode=flat',
            'title' => 'Test Comment #1_3_1_1 >>'
        }
    ],
    '/article/19700101010000/3?cid=12345/6/7/8' => [
        {   'href'  => '/article/12345/6/7/8',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8',
            'title' => 'Reply to this comment'
        }
    ],
    '/article/19700101010000/3?cid=12345/6/7/8&mode=collapsed' => [
        {   'href'  => '/article/12345/6/7/8?mode=collapsed',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8?mode=collapsed',
            'title' => 'Reply to this comment'
        }
    ],
    '/article/19700101010000/3?cid=12345/6/7/8&mode=expanded' => [
        {   'href'  => '/article/12345/6/7/8?mode=expanded',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8?mode=expanded',
            'title' => 'Reply to this comment'
        }
    ],
    '/article/19700101010000/3?cid=12345/6/7/8&mode=flat' => [
        {   'href'  => '/article/12345/6/7/8?mode=flat',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8?mode=flat',
            'title' => 'Reply to this comment'
        }
    ],
    '/article/19700101010000/3?mode=collapsed' => [
        {   'href'  => '/article/19700101010000/2?mode=collapsed',
            'title' => '<< Test Comment #2'
        },
        {   'href'  => '/article/19700101010000?mode=collapsed',
            'title' => 'Up: Test Post #1'
        },
        {   'href'  => '/article/19700101010000/3?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101010000/3?mode=expanded',
            'title' => 'Expanded'
        },
        {   'href'  => '/article/19700101010000/3/1?mode=collapsed',
            'title' => 'Test Comment #1_3_1 >>'
        }
    ],
    '/article/19700101010000/3?mode=expanded' => [
        {   'href'  => '/article/19700101010000/2?mode=expanded',
            'title' => '<< Test Comment #2'
        },
        {   'href'  => '/article/19700101010000?mode=expanded',
            'title' => 'Up: Test Post #1'
        },
        {   'href'  => '/article/19700101010000/3?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101010000/3',
            'title' => 'Collapsed'
        },
        {   'href'  => '/article/19700101010000/3/1?mode=expanded',
            'title' => 'Test Comment #1_3_1 >>'
        }
    ],
    '/article/19700101010000/3?mode=flat' => [
        {   'href'  => '/article/19700101010000/2?mode=flat',
            'title' => '<< Test Comment #2'
        },
        {   'href'  => '/article/19700101010000?mode=flat',
            'title' => 'Up: Test Post #1'
        },
        {   'href'  => '/article/19700101010000/3?mode=expanded',
            'title' => 'Expanded'
        },
        {   'href'  => '/article/19700101010000/3',
            'title' => 'Collapsed'
        },
        {   'href'  => '/article/19700101010000/3/1?mode=flat',
            'title' => 'Test Comment #1_3_1 >>'
        }
    ],
    '/article/19700101010000?cid=12345/6/7/8' => [
        {   'href'  => '/article/12345/6/7/8',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8',
            'title' => 'Reply to this comment'
        }
    ],
    '/article/19700101010000?cid=12345/6/7/8&mode=collapsed' => [
        {   'href'  => '/article/12345/6/7/8?mode=collapsed',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8?mode=collapsed',
            'title' => 'Reply to this comment'
        }
    ],
    '/article/19700101010000?cid=12345/6/7/8&mode=expanded' => [
        {   'href'  => '/article/12345/6/7/8?mode=expanded',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8?mode=expanded',
            'title' => 'Reply to this comment'
        }
    ],
    '/article/19700101010000?cid=12345/6/7/8&mode=flat' => [
        {   'href'  => '/article/12345/6/7/8?mode=flat',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8?mode=flat',
            'title' => 'Reply to this comment'
        }
    ],
    '/article/19700101010000?mode=collapsed' => [
        {   'href'  => '/article/19700101010000?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101010000?mode=expanded',
            'title' => 'Expanded'
        },
        {   'href'  => '/article/19700101020000?mode=collapsed',
            'title' => 'Test Post #2 >>'
        }
    ],
    '/article/19700101010000?mode=expanded' => [
        {   'href'  => '/article/19700101010000?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101010000',
            'title' => 'Collapsed'
        },
        {   'href'  => '/article/19700101020000?mode=expanded',
            'title' => 'Test Post #2 >>'
        }
    ],
    '/article/19700101010000?mode=flat' => [
        {   'href'  => '/article/19700101010000?mode=expanded',
            'title' => 'Expanded'
        },
        {   'href'  => '/article/19700101010000',
            'title' => 'Collapsed'
        },
        {   'href'  => '/article/19700101020000?mode=flat',
            'title' => 'Test Post #2 >>'
        }
    ],
    '/article/19700101020000' => [
        {   'href'  => '/article/19700101010000',
            'title' => '<< Test Post #1'
        },
        {   'href'  => '/article/19700101020000?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101020000?mode=expanded',
            'title' => 'Expanded'
        },
        {   'href'  => '/article/19700101030000',
            'title' => 'Test Post #3 >>'
        }
    ],
    '/article/19700101020000?cid=12345/6/7/8' => [
        {   'href'  => '/article/12345/6/7/8',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8',
            'title' => 'Reply to this comment'
        }
    ],
    '/article/19700101020000?cid=12345/6/7/8&mode=collapsed' => [
        {   'href'  => '/article/12345/6/7/8?mode=collapsed',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8?mode=collapsed',
            'title' => 'Reply to this comment'
        }
    ],
    '/article/19700101020000?cid=12345/6/7/8&mode=expanded' => [
        {   'href'  => '/article/12345/6/7/8?mode=expanded',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8?mode=expanded',
            'title' => 'Reply to this comment'
        }
    ],
    '/article/19700101020000?cid=12345/6/7/8&mode=flat' => [
        {   'href'  => '/article/12345/6/7/8?mode=flat',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8?mode=flat',
            'title' => 'Reply to this comment'
        }
    ],
    '/article/19700101020000?mode=collapsed' => [
        {   'href'  => '/article/19700101010000?mode=collapsed',
            'title' => '<< Test Post #1'
        },
        {   'href'  => '/article/19700101020000?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101020000?mode=expanded',
            'title' => 'Expanded'
        },
        {   'href'  => '/article/19700101030000?mode=collapsed',
            'title' => 'Test Post #3 >>'
        }
    ],
    '/article/19700101020000?mode=expanded' => [
        {   'href'  => '/article/19700101010000?mode=expanded',
            'title' => '<< Test Post #1'
        },
        {   'href'  => '/article/19700101020000?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101020000',
            'title' => 'Collapsed'
        },
        {   'href'  => '/article/19700101030000?mode=expanded',
            'title' => 'Test Post #3 >>'
        }
    ],
    '/article/19700101020000?mode=flat' => [
        {   'href'  => '/article/19700101010000?mode=flat',
            'title' => '<< Test Post #1'
        },
        {   'href'  => '/article/19700101020000?mode=expanded',
            'title' => 'Expanded'
        },
        {   'href'  => '/article/19700101020000',
            'title' => 'Collapsed'
        },
        {   'href'  => '/article/19700101030000?mode=flat',
            'title' => 'Test Post #3 >>'
        }
    ],
    '/article/19700101030000' => [
        {   'href'  => '/article/19700101020000',
            'title' => '<< Test Post #2'
        },
        {   'href'  => '/article/19700101030000?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101030000?mode=expanded',
            'title' => 'Expanded'
        }
    ],
    '/article/19700101030000?cid=12345/6/7/8' => [
        {   'href'  => '/article/12345/6/7/8',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8',
            'title' => 'Reply to this comment'
        }
    ],
    '/article/19700101030000?cid=12345/6/7/8&mode=collapsed' => [
        {   'href'  => '/article/12345/6/7/8?mode=collapsed',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8?mode=collapsed',
            'title' => 'Reply to this comment'
        }
    ],
    '/article/19700101030000?cid=12345/6/7/8&mode=expanded' => [
        {   'href'  => '/article/12345/6/7/8?mode=expanded',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8?mode=expanded',
            'title' => 'Reply to this comment'
        }
    ],
    '/article/19700101030000?cid=12345/6/7/8&mode=flat' => [
        {   'href'  => '/article/12345/6/7/8?mode=flat',
            'title' => 'Show Thread'
        },
        {   'href'  => '/reply/12345/6/7/8?mode=flat',
            'title' => 'Reply to this comment'
        }
    ],
    '/article/19700101030000?mode=collapsed' => [
        {   'href'  => '/article/19700101020000?mode=collapsed',
            'title' => '<< Test Post #2'
        },
        {   'href'  => '/article/19700101030000?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101030000?mode=expanded',
            'title' => 'Expanded'
        }
    ],
    '/article/19700101030000?mode=expanded' => [
        {   'href'  => '/article/19700101020000?mode=expanded',
            'title' => '<< Test Post #2'
        },
        {   'href'  => '/article/19700101030000?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101030000',
            'title' => 'Collapsed'
        }
    ],
    '/article/19700101030000?mode=flat' => [
        {   'href'  => '/article/19700101020000?mode=flat',
            'title' => '<< Test Post #2'
        },
        {   'href'  => '/article/19700101030000?mode=expanded',
            'title' => 'Expanded'
        },
        {   'href'  => '/article/19700101030000',
            'title' => 'Collapsed'
        }
    ],
    '19700101010000' => [
        {   'href'  => '/article/19700101010000?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101010000?mode=expanded',
            'title' => 'Expanded'
        }
    ],
    '19700101010000/3' => [
        {   'href'  => '/article/19700101010000',
            'title' => 'Up: Test Post #1'
        },
        {   'href'  => '/article/19700101010000/3?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101010000/3?mode=expanded',
            'title' => 'Expanded'
        }
    ],
    '19700101010000/3/1' => [
        {   'href'  => '/article/19700101010000/3',
            'title' => 'Up: Test Comment #1_3'
        },
        {   'href'  => '/article/19700101010000/3/1?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101010000/3/1?mode=expanded',
            'title' => 'Expanded'
        }
    ],
    '19700101010000/3/1/1' => [
        {   'href'  => '/article/19700101010000/3/1',
            'title' => 'Up: Test Comment #1_3_1'
        },
        {   'href'  => '/article/19700101010000/3/1/1?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101010000/3/1/1?mode=expanded',
            'title' => 'Expanded'
        }
    ],
    '19700101020000' => [
        {   'href'  => '/article/19700101020000?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101020000?mode=expanded',
            'title' => 'Expanded'
        }
    ],
    '19700101030000' => [
        {   'href'  => '/article/19700101030000?mode=flat',
            'title' => 'Flattened'
        },
        {   'href'  => '/article/19700101030000?mode=expanded',
            'title' => 'Expanded'
        }
    ]
);

foreach my $id (@ids) {
    check_links( $t->app, $id );

    foreach my $cid ( '', @cids ) {
        foreach my $mode ( '', @modes ) {

            my $params = '';
            $params .= "cid=$cid"   if $cid;
            $params .= '&'          if $params and $mode;
            $params .= "mode=$mode" if $mode;

            $t->get_ok( "/?id=$id" .     ( $params ? "&$params" : '' ) );
            $t->get_ok( "/article/$id" . ( $params ? "?$params" : '' ) );
        }
    }
}

done_testing;

sub navlinks_for {
    my $app = shift;

    # flatten to make is_deeply work more reliably
    # here we assume that $_->{href} isa Mojo::URL
    return [
        map {
            $_->{href} = "$_->{href}";
            $_->{title} =~ s/\d+d ago/Xd ago/;
            $_
        } $app->navigation(@_)
    ];
}

sub check_links {
    my ( $app, $url ) = @_;

    my $id = $url;
    my $cid;

    if ( ref $url ) {
        $cid = $url->query->param('cid');
        $id = $url->query->param('id') || "$url";
        if ( $id =~ m{^/article/([^?]+)} ) {
            $id = $1;
        }
    }

    my $got = eval { navlinks_for( $app, "$id", $cid ) };
    my $expect = $links{"$url"};

    is_deeply $got, $expect, "[$url] has expected links";
}
