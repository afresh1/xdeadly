package XDeadly::Post;
use Mojo::Base 'Mojo::Message';

our $VERSION = '0.01';

use Mojo::Asset::File;
use Mojo::Asset::Memory;
use Mojo::Content::Single;

use Time::Local qw(timegm);
use XDeadly::Comment;

use Carp;
use File::Spec::Functions qw/ catdir splitpath /;

has content => sub {
    my ($self) = @_;

    $self->content( Mojo::Content::Single->new );

    if ($self->path) {
        my $file = Mojo::Asset::File->new( path => $self->path );
        $self->parse( $file->slurp );
    }

    return $self->content;
};

has 'path';
has '_epoch';

sub parse {
    my $self = shift->SUPER::parse(@_);

    if (my $date = $self->headers->date) {
        $self->_epoch( _parse_ctime($date) );
    }

    return $self;
}

sub _parse_ctime {
    my ($string) = @_;

    my %months = (
        Jan => 0,
        Feb => 1,
        Mar => 2,
        Apr => 3,
        May => 4,
        Jun => 5,
        Jul => 6,
        Aug => 7,
        Sep => 8,
        Oct => 9,
        Nov => 10,
        Dec => 11,
    );

    my ($wday, $mname, $day, $time, $year) = split /\s+/xms, $string;
    my ($h, $m, $s) = split /:/xms, $time;

    return timegm( $s, $m, $h, $day, $months{$mname}, $year );
};

sub dir {
    my ($self) = @_;
    croak('path not defined') unless $self->path;

    my ($vol, $dirs, $file) = File::Spec->splitpath( $self->path );
    return File::Spec->canonpath( File::Spec->catpath( $vol, $dirs ) );
}

sub save {
    my ($self, $path) = @_;
    $path ||= $self->path;

    croak("Cannot save without a path") unless $path;
    $self->path($path);

    my $file = Mojo::Asset::Memory->new();
    $file->add_chunk( $self->to_string );
    $file->move_to($path);

    return $self;
}

# We don't actually have a start_line
sub extract_start_line {1}

sub get_start_line_chunk {
    my ( $self, $offset ) = @_;

    # need start_buffer to be defined, should only ever be the empty string
    $self->{start_buffer} //= q{};

    $self->emit( progress => 'start_line', $offset );
    return substr $self->{start_buffer}, $offset, 131_072;
}


1;
