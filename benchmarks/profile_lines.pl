#!/usr/bin/perl
# usage: profile_lines.pl <binary> <perf script output>
# Rewrites every frame of <binary> as its inline chain, outermost first: fn@file:line->fn@file:line.
# stackcollapse-perf.pl splits on "->", so inlined functions become frames of their own.
# All addresses go through one addr2line call; perf's own srcline forks one per frame (and fails on kcore).
# Expects perf script --no-inline -F ...,sym,symoff,dso output of a --call-graph dwarf recording.
use strict;
use warnings;

my ($bin, $in) = @ARGV;
my $frame = qr/^(\s+)([0-9a-f]+) (\S+)\+0x([0-9a-f]+) \(\Q$bin\E\)$/;

# perf prints runtime addresses on some versions and file offsets on others, so the file address
# addr2line needs is rebuilt from symbol + offset instead.
my %sym;
open my $nm, '-|', 'nm', '--defined-only', $bin or die "nm: $!";
while (<$nm>)
{
    $sym{$2} = hex($1) if /^([0-9a-f]+) \S (\S+)$/;
}
close $nm;

sub file_addr
{
    my ($name, $off) = @_;
    return exists $sym{$name} ? sprintf("%x", $sym{$name} + hex($off)) : undef;
}

my %chain;
open my $f, '<', $in or die "$in: $!";
while (<$f>)
{
    next unless /$frame/;
    my $addr = file_addr($3, $4);
    $chain{$addr} = '' if defined $addr;
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
        next if $fn eq '??';
        push @fns, $line =~ /^[^?].*:[1-9]\d*$/ ? "$fn\@$line" : $fn;
    }
    $chain{$addr} = join('->', reverse @fns) if defined $addr;
    close $a2l;
}

open $f, '<', $in or die "$in: $!";
while (<$f>)
{
    if (/$frame/)
    {
        my $addr = file_addr($3, $4);
        $_ = "$1$2 $chain{$addr} ($bin)\n" if defined $addr && $chain{$addr} ne '';
    }
    print;
}
