package XDeadlyFixtures;
use Mojo::Base -base;

use Carp;
use File::Basename qw( dirname );
use File::Path qw( make_path );
use File::Spec;
use File::Find;
use File::Copy;

sub copy_fixtures {
    my ($dest_dir) = @_;

    croak 'Destination dir is required' unless $dest_dir;

    my $source_dir = File::Spec->catdir( dirname(__FILE__), File::Spec->updir,
        'fixtures', 'standard' );

    find( sub {
        my $path = File::Spec->catdir( $dest_dir,
            File::Spec->abs2rel( $File::Find::name, $source_dir ) );

        if (-d $_) {
            make_path( $path ) or die "Couldn't make_path( $path ): $!"
                unless -d $path;
        }
        else {
            copy $_, $path or die "Couldn't copy $File::Find::name: $!";
        }

    }, $source_dir );
}

1;
