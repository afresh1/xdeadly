package XDeadly::PostMod;
use Mojo::Base -base;

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
