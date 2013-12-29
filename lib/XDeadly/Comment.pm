package XDeadly::Comment;
use Mojo::Base 'XDeadly::Post';

our $VERSION = 0.01;

use XDeadly::Article;

use File::Spec::Functions qw(catfile catdir);

has filename => 'comment';

has 'cid' => sub {
    my ($self) = @_;
    return unless $self->parent;

    if ($self->{id}) {
        my $parent_id = $self->parent->id;
        my $cid = $self->id;
        $cid =~ s{^$parent_id/}{};
    }

    my $parent_dir = $self->parent->dir;

    my $cid = 1;
    $cid++ while -e catdir $parent_dir, $cid, $self->filename;
    mkdir catdir $parent_dir, $cid;

    return $cid;
};

# id's are joined with /, not the file path separator.
# This is because it is representative of a URI path
has id => sub {
    my $self = shift;
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
    return $parent->is_article ? $parent : $parent->article;
}

1;
