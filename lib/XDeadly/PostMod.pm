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

has 'post';

has _points => sub {
    my ($self) = @_;
    return unless $self->post;

    my %mod = (
        sum   => 0,
        count => 0,
        votes => {},
    );
    return \%mod unless -e $self->path;

    my $file = Mojo::Asset::File->new( path => $self->path );

    foreach ( split /\n/, $file->slurp ) {
        my ( $who, $what ) = split /\s+/;

        $mod{votes}{$who} = $what;
        next unless $what;

        $mod{count}++;
        $mod{sum} += $what;
    }

    return \%mod
};

sub path { return shift->post->path . '.mod' }

sub vote {
    my ($self, $who, $what) = @_;

    my $mod = delete $self->{_points};
    $mod->{votes}->{$who} = $what;

    open my $fh, '>', $self->path or die "Couldn't update modfile: $!";
    foreach my $who (keys %{ $mod->{votes} }) {
        print $fh "$who $mod->{votes}->{$who}\n";
    }
    close $fh;

    return $self;
}

sub score { shift->_points->{sum} }
sub votes { shift->_points->{count} }
sub has_voted { 
    my ($self, $who) = @_;
    my $votes = $self->_points->{votes};
    return exists $votes->{$who};
}

1;
