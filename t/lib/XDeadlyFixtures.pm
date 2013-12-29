package XDeadlyFixtures;
use Mojo::Base -base;

use Carp;
use File::Basename qw( dirname );
use File::Path qw( make_path );
use File::Spec;
use File::Find;
use File::Copy;

sub _copy_fixtures {
    my ($rel_src, $dest_dir) = @_;

    croak 'Destination dir is required' unless $dest_dir;

    my $source_dir = File::Spec->catdir( dirname(__FILE__), File::Spec->updir,
        'fixtures', @{ $rel_src || [] } );

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

sub copy_good_data_fixtures { _copy_fixtures(['good','data'],@_) }
sub copy_good_auth_fixtures { _copy_fixtures(['good','auth'],@_) }

1;
