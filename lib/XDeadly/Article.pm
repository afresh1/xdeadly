package XDeadly::Article;
use Mojo::Base 'XDeadly::Post';

our $VERSION = 0.01;

use Carp;
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
