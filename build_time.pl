use strict;
use warnings;
use POSIX qw(strftime);

my $output = "build_time.h";

# UTC 기준 현재 시각 구한 뒤 +9시간
my $now = time() + 9 * 60 * 60;

my @tm = gmtime($now);
my $build_time_kst = strftime("%Y-%m-%d %H:%M:%S", @tm);

open(my $fh, ">:encoding(UTF-8)", $output)
    or die "cannot open $output for write: $!";

print $fh "#pragma once\n";
print $fh "#ifndef RLAQHADMLGPEJWNDQHRQKDWLAOZMFH_BUILD_TIME_H\n";
print $fh "#define RLAQHADMLGPEJWNDQHRQKDWLAOZMFH_BUILD_TIME_H\n";
print $fh "#define BUILD_TIME_KST \"$build_time_kst\"\n";
print $fh "#endif\n";
close($fh);