package XDeadly::Post;
use Mojo::Base 'Mojo::Message';

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
use Mojo::Asset::Memory;
use Mojo::Content::Single;

use Time::Local qw(timegm);
use XDeadly::Article;
use XDeadly::Comment;
use XDeadly::PostMod;

use Carp;
use File::Spec::Functions qw/ canonpath catdir catfile /;
use File::Path qw/ make_path /;

use overload
    '""' => sub { shift->id },
    fallback => 1;

has content => sub {
    my ($self) = @_;

    $self->content( Mojo::Content::Single->new );

    if ($self->has_path and -e $self->path) {
        my $file = Mojo::Asset::File->new( path => $self->path );
        $self->parse( $file->slurp );
    }

    return $self->content;
};

has mod => sub { XDeadly::PostMod->new( post => shift ) };

has '_epoch' => sub {
    my ($self) = @_;
    return unless $self->headers->date;
    return _parse_ctime( $self->headers->date );
};

has level => 0;
has is_article => 0;

has 'id';
has 'filename' => 'post';
has 'data_dir';

sub subject { shift->content->headers->header( subject => @_ ) }
sub name    { shift->content->headers->header( name    => @_ ) }
sub email   { shift->content->headers->header( email   => @_ ) }
sub href    { shift->content->headers->header( href    => @_ ) }
sub host    { shift->content->headers->header( host    => @_ ) }

sub date { shift->content->headers->header( date => @_ ) . ' (GMT)' }
sub time { sprintf "%02d:%02d:%02d", ( gmtime( shift->_epoch ) )[ 2, 1, 0 ] }

sub short_date {
    my ($self) = @_;

    my @days = ( qw(
        Sunday
        Monday
        Tuesday
        Wednesday
        Thursday
        Friday
        Saturday
    ) );

    my @months = ( qw(
        January
        February
        March
        April
        May
        June
        July
        August
        September
        October
        November
        December
    ) );

    my @d = gmtime( $self->_epoch );
    return sprintf "%s, %s %d", $days[ $d[6] ], $months[ $d[4] ], $d[3];
}

sub parse {
    my ( $self, $chunk ) = @_;

    $self->content->on( body => sub {
        my $content = shift;
        my $headers = $content->headers;

        $headers->content_length( length $content->{pre_buffer} )
            unless $headers->content_length;
    } );

    return $self->SUPER::parse($chunk);
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

    my ($wday, $mname, $day, $time, $year, $extra) = split /\s+/xms, $string;
    croak "Invalid date [$string]" if $extra;

    my ($h, $m, $s) = split /:/xms, $time;

    return timegm( $s, $m, $h, $day, $months{$mname}, $year );
};

=head3 dir

The working directory where we store our files.

Should be data_dir/id

=cut

sub dir {
    my ($self) = @_;
    croak('data_dir and id required') unless $self->data_dir and $self->id;
    return canonpath( catdir $self->data_dir, split '/', $self->id );
}

=head3 has_path

A check whether we can calculate the path

=cut

sub has_path {
    my ($self) = @_;
    return $self->id and $self->data_dir
}

=head3 path

The full path to the content of the post.

Built from data_dir/id/filename

=cut

sub path {
    my ($self) = @_;
    return catfile $self->dir, $self->filename;
}

=head3 save( data_dir => $data_dir, id => $id )

saves the file, optionally takes data_dir and id parameters

=cut

sub save {
    my ($self, %args) = @_;

    $self->id( $args{id} )             if $args{id};
    $self->data_dir( $args{data_dir} ) if $args{data_dir};

    croak("Cannot save without a path") unless $self->has_path;

    my $file = Mojo::Asset::Memory->new();
    $file->add_chunk( $self->to_string );

    make_path $self->dir;
    $file->move_to($self->path);

    return $self;
}

sub latest_change {
    my ($self) = @_;

    return [ sort { $a <=> $b }
            ( $self->_epoch, map { $_->_epoch } @{ $self->comments } ) ]->[-1];
}

sub articles {
    my ( $self, $data_dir ) = @_;
    return [ $self->_load_articles($data_dir) ];
};

sub _load_articles {
    my ( $self, $dir ) = @_;

    my $filename = XDeadly::Article->new->filename;

    return map { $self->_load_article( $dir, $_ ) }
        sort { $b <=> $a }    # newest to oldest
        grep { -f catfile $dir, $_, $filename } # only dirs with an article in them
        $self->_posts($dir);
}

sub _posts {
    my ( $self, $dir ) = @_;

    opendir my $dh, $dir or croak "Couldn't opendir $dir: $!";
    my @posts = grep { -d catdir( $dir, $_ ) }    # only dirs that exist
        grep {/^[^\.]/}                           # No dotfiles
        readdir $dh;
    closedir $dh or croak "Couldn't closedir $dir: $!";

    return @posts;
}

sub _load_article {
    my ( $self, $data_dir, $id ) = @_;
    XDeadly::Article->new( data_dir => $data_dir, id => $id );
}

sub comments {
    my ($self) = @_;
    return [ map { ( $_, @{ $_->comments } ) }
            $self->_load_comments( $self->dir ) ];
}

sub _load_comments {
    my ( $self, $dir ) = @_;

    my $filename = XDeadly::Comment->new->filename;

    return map { $self->_load_comment( $dir, $_ ) }
        sort { $a <=> $b }    # oldest to newest
        grep { -f catfile $dir, $_, $filename } # only dirs with a comment in them
        $self->_posts($dir);
}

sub _load_comment {
    my ( $self, $dir, $cid ) = @_;

    return XDeadly::Comment->new(
        article => $self,
        parent  => $self,
        cid     => $cid,
    );
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
