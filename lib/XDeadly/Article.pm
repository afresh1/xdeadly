package XDeadly::Article;
use Mojo::Base 'XDeadly::Post';

our $VERSION = 0.01;

has filename => 'article';

has is_article => 1;

has 'id' => sub {
    my ($self) = @_;

    my @now = (gmtime)[0..5];
    $now[4]++;
    $now[5] += 1900;

    return sprintf '%04d%02d%02d%02d%02d%02d', reverse @now;
};

1;
