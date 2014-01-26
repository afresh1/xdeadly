package XDeadly::Article;
use Mojo::Base 'XDeadly::Post';

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

our $VERSION = 0.01;

use Carp;
use File::Spec::Functions qw(catfile);
use Mojo::Asset::File;

has filename => 'article';
has more_filename => sub { shift->filename . '.more' };

has is_article => 1;

has 'id' => sub {
    my ($self) = @_;
    return _epoch_to_id(time);
};

sub _epoch_to_id {
    my ($epoch) = @_;

    my @now = (gmtime($epoch))[0..5];
    $now[4]++;
    $now[5] += 1900;

    return sprintf '%04d%02d%02d%02d%02d%02d', reverse @now;
}

sub has_more {
    my $self = shift;
    return !!$self->{more} if exists $self->{more};
    return -s $self->more_path;
}

has more_path => sub {
    my $self = shift;
    catfile $self->dir, $self->more_filename;
};

has more => sub {
    my ($self) = @_;
    return unless $self->data_dir;
    return unless $self->has_more;

    my $more = Mojo::Asset::File->new( path => $self->more_path )->slurp;

    return $more;
};

sub save {
    my $self = shift;
    $self->SUPER::save(@_);

    if ($self->{more}) {
        my $file = Mojo::Asset::Memory->new();

        $file->add_chunk( $self->more );

        $file->move_to($self->more_path);
    }
    elsif ( -e $self->more_path ) {
        unlink $self->more_path || croak "Couldn't remove more file: $!";
    }

    return $self;
}

sub department { shift->headers->header( Dept  => @_ ) }
sub topic      { shift->headers->header( Topic => @_ ) }
sub topicquery { shift->topic(@_) }

sub topicimg {
    my ($self) = @_;

    my $topic = $self->topic || return;

    my %topicext;

    $topicext{$_} = 'gif' for qw(
        oreilly_weasel
        topicbl
        topicbsd
        topiccrypto
        topiceditorial
        topicmail
        topicopenbsd
        topicopenssh
        topicports
    );

    $topicext{$_} = 'png' for qw(
        topic30
        topic30
        topicpf2
        topicreadme
        topicnda
        topicsparc
    );

    my $ext = $topicext{$topic} || 'jpg';

    return "$topic.$ext";
}

1;
