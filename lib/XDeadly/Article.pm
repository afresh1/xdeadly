package XDeadly::Article;
use Mojo::Base 'XDeadly::Post';

our $VERSION = 0.01;

use File::Spec::Functions qw(catfile);
use Mojo::Asset::File;

has filename => 'article';
has more_filename => sub { shift->filename . '.more' };

has is_article => 1;

has 'id' => sub {
    my ($self) = @_;

    my @now = (gmtime)[0..5];
    $now[4]++;
    $now[5] += 1900;

    return sprintf '%04d%02d%02d%02d%02d%02d', reverse @now;
};

sub has_more { my $self = shift; !!$self->{more} || -s $self->more_path }

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

    return $self;
}

1;
