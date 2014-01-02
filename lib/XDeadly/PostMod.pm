package XDeadly::PostMod;
use Mojo::Base -base;

# Copyright (c) 2013 Andrew Fresh <andrew@afresh1.com>
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

our $VERSION = '0.01';

use Mojo::Asset::File;
use List::Util qw( sum );

has 'post';

has _points => sub {
    my ($self) = @_;

    my %mod = ();
    return \%mod unless $self->path and -e $self->path;

    my $file = Mojo::Asset::File->new( path => $self->path );

    foreach ( split /\n/, $file->slurp ) {
        my ( $who, $what ) = split /\s+/;

        $mod{$who} = $what;
    }

    return \%mod
};

sub path { 
    my $post = shift->post;
    return unless $post;
    return $post->path . '.mod';
}

sub vote {
    my ($self, $who, $what) = @_;

    my $mod = $self->_points->{$who} = $what;

    $self->_save if $self->path;

    return $self;
}

sub _save {
    my ($self) = @_;

    open my $fh, '>', $self->path or die "Couldn't update modfile: $!";

    my $mod = delete $self->{_points};
    print $fh $_ for map { "$_ $mod->{$_}\n" } sort keys %{ $mod };

    close $fh;

    return $self;
}

sub score { sum( values %{ shift->_points }, 0 ) }

sub votes {
    scalar grep {$_} values %{ shift->_points };
}

sub has_voted {
    my ( $self, $who ) = @_;
    return exists $self->_points->{$who};
}

1;
