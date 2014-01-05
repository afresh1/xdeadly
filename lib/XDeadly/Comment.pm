package XDeadly::Comment;
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

use XDeadly::Article;
use Carp;

use File::Spec::Functions qw(catfile catdir);

has filename => 'comment';

has 'cid' => sub {
    my ($self) = @_;
    return unless $self->parent;

    my $cid = 1;

    if ($self->{id}) {
        my $parent_id = $self->parent->id;
        $cid = $self->id;
        $cid =~ s{^\Q$parent_id/}{}
            || croak "Cannot calculate cid: parent [$parent_id] and ID [$cid] incompatable";
    }
    else {
        my $parent_dir = $self->parent->dir;
        $cid++ while -e catdir $parent_dir, $cid;
    }

    return $cid;
};

# id's are joined with /, not the file path separator.
# This is because it is representative of a URI path
has id => sub {
    my ($self) = @_;
    return unless $self->parent;
    return join '/', $self->parent->id, $self->cid;
};
has 'parent' => sub {
    my ($self) = @_;
    return unless $self->{id};    # avoid the recursion

    my $id = $self->id;
    $id =~ s{/[^/]+/?$}{}; # strip the last element

    return $id =~ m{/}
        ? XDeadly::Comment->new( data_dir => $self->data_dir, id => $id )
        : XDeadly::Article->new( data_dir => $self->data_dir, id => $id );
};

has 'data_dir' => sub {
    my ($self) = @_;
    return unless $self->parent;
    return $self->parent->data_dir;
};

sub article {
    my $parent = shift->parent;
    return unless $parent;
    return $parent->is_article ? $parent : $parent->article;
}

sub level { shift->parent->level + 1 }

1;
