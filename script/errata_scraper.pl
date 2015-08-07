#!/usr/bin/env perl
########################################################################
# Copyright (c) 2011 Andrew Fresh <andrew@afresh1.com>
# Copyright (c) 2015 Tim Howe <timh@dirtymonday.net>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
########################################################################
use strict;
use warnings;
use autodie;

use feature 'postderef';
no warnings 'experimental::postderef';

use Time::Piece;
use Mojo::UserAgent;
use Mojo::URL;
use Mojo::JSON qw(encode_json);

my $base_uri = 'http://www.openbsd.org/';

my $ua = Mojo::UserAgent->new;

my $ls = $ua->get( $base_uri . 'errata.html' )->res->dom('a[href^="errata"]');

my $errata = [];

foreach my $l ( $ls->@[ -2, -1 ] ) {
    my $vertitle = 'Errata for OpenBSD ' . $l->text;
    my $entries = [];
    foreach my $e (
        reverse $ua->get( $base_uri . $l->attr->{'href'}
                         )->res->dom('body > ul > li')->@*
                  )
    {
        my $link  = Mojo::URL->new( $base_uri . $l->attr->{'href'}
                                  )->fragment($e->attr('id'));
        my $patch = $e->at('a[href$=".patch.sig"]')->attr('href') if
                        defined $e->at('a[href$=".patch.sig"]');
        my $title = $e->at('font > strong')->text;
        my ($pnum, $category, $rawdate) = $title =~ /\A(\d+):\s+(\w+).*:\s(.+)\z/;
        my $date = Time::Piece->strptime($rawdate, "%B %d, %Y");
        my $arch  = $e->at('i')->text;

        ## FIXME! This needs to be something that might work better. ##
        ## I recommend improving the structure of the errata pages. --TimH ##
        # For description I just use everything after the first <br> tag.
        # That probably will only work until someone starts typing it
        # different.
        # This leaves the HTML intact for the description.
        my $descr = $e->to_string;
        $descr =~ s/.*?<br>//s;
        ##

        $descr =~ s/\A\s+//;
        $descr =~ s/\s+/ /gs;
        $descr =~ s/\s(\.(?:\s|$))/$1/gs;
        $descr =~ s/\.+$/./gs;

        push $entries->@*, {
            'link'     => $link->to_string,
            'title'    => $title,
            'date'     => $date->strftime("%d %B %Y"),
            'arch'     => $arch,
            'category' => $category,
            'patch'    => defined $patch ? $patch : '',
            'descr'    => $descr, };
    }

    push $errata->@*, { $vertitle => $entries };
}

open my $json_file, '>', 'public/errata.json';

print $json_file encode_json($errata);

close $json_file;

