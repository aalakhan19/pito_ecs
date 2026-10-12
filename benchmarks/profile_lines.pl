#!/usr/bin/perl
# usage: profile_lines.pl <binary> <perf script output>
# Rewrites every frame of <binary> as its inline chain, outermost first: fn@file:line->fn@file:line.
# stackcollapse-perf.pl splits on "->", so inlined functions become frames of their own.
# All addresses go through one addr2line call; perf's own srcline forks one per frame (and fails on kcore).
# Expects perf script --no-inline output of a --call-graph dwarf recording (caller addresses already - 1).
use strict;
use warnings;

my ($bin, $in) = @ARGV;
my $frame = qr/^(\s+)([0-9a-f]+) (\S+) \(\Q$bin\E\)$/;

my %chain;
open my $f, '<', $in or die "$in: $!";
while (<$f>)
{
    $chain{$2} = undef if /$frame/;
}
close $f;

my @addrs = keys %chain;
if (@addrs)
{
    open my $a2l, '-|', 'addr2line', '-e', $bin, '-a', '-f', '-i', '-s', '-C', @addrs or die "addr2line: $!";
    my ($addr, @fns);
    while (my $fn = <$a2l>)
    {
        chomp $fn;
        if ($fn =~ /^0x0*([0-9a-f]*)$/)
        {
            $chain{$addr} = join('->', reverse @fns) if defined $addr;
            ($addr, @fns) = ($1 eq '' ? '0' : $1);
            next;
        }
        my $line = <$a2l>;
        chomp $line;
        $line =~ s/ \(discriminator \d+\)//;
        push @fns, $line =~ /^[^?].*:[1-9]\d*$/ ? "$fn\@$line" : $fn;
    }
    $chain{$addr} = join('->', reverse @fns) if defined $addr;
    close $a2l;
}

open $f, '<', $in or die "$in: $!";
while (<$f>)
{
    if (/$frame/ && defined $chain{$2} && $chain{$2} ne '')
    {
        $_ = "$1$2 $chain{$2} ($bin)\n";
    }
    print;
}
