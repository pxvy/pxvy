use strict;
use warnings;

my $file = "version.h";

open(my $in, "<:encoding(UTF-8)", $file) or die $!;
my @lines = <$in>;
close($in);

for (@lines) {
    s/#define VERSION\s+"(\d+)\.(\d+)\.(\d+)"/
        '#define VERSION "' . $1 . '.' . $2 . '.' . ($3 + 1) . '"'/e;
}

open(my $out, ">:encoding(UTF-8)", $file) or die $!;
print $out @lines;
close($out);