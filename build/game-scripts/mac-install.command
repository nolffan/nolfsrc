#!/usr/bin/perl
# mac-install.command -- install the game data from your two NOLF discs, on a
# Mac (to play the game with CrossOver, or another Wine).
#
#   Double-click it in Finder with both disc images in this folder (or the
#   one above it), or in Terminal:
#   ./mac-install.command <disc image> <disc image>
#
# The images are the two discs of The Operative: No One Lives Forever, Game
# of the Year Edition (Europe), in either order, each as any of
#
#   .iso   a plain image (2048-byte sectors), e.g. your own rip
#   .bin   a raw image (2352-byte sectors), or its .cue
#   .zip   a zip of the .cue and the .bin
#
# What linux-install.sh does: a .bin, .cue or .zip is first converted to a
# temporary .iso in this folder (about 0.6 GB a disc, removed at the end).
# First it checks that this folder is the release unpacked, and nothing
# else: every file SHA256SUMS lists is here and unchanged, and no file is
# here that the release does not have (it refuses a release copied over
# another installation). Then it checks each .iso's SHA-256, copies the
# game's data out of them into this folder, and checks every file it writes;
# nothing else is changed. It can be run again: a file already in place is
# checked and kept.
#
# It needs only the Perl every Mac has (/usr/bin/perl, with its own modules).
# The list of files, at the end of this script, says for each one which
# bytes of which disc it is made of (build/mkinstall.py).
use strict;
use warnings;
use Cwd qw(abs_path);
use File::Basename qw(basename dirname);
use File::Find ();
use File::Path qw(make_path);
use Digest::SHA;
use MIME::Base64 qw(decode_base64);
use Compress::Raw::Zlib;

$| = 1;
my $ME = 'mac-install';
my $SELF = abs_path($0);
my $HERE = dirname($SELF);
my @TEMPS;    # converted images, removed however the script ends

sub cleanup { unlink grep { -e } @TEMPS; @TEMPS = (); }
END { cleanup(); }
$SIG{INT} = $SIG{TERM} = $SIG{HUP} = sub { cleanup(); exit 1; };
sub fail { print STDERR "\n$ME: $_[0]\n"; cleanup(); exit 1; }

sub sha256_file {
    my $d = Digest::SHA->new(256);
    $d->addfile($_[0], 'b') or fail("$_[0]: cannot read it");
    return $d->hexdigest;
}

# The list: every line after the marker line.
my @data;
{
    open my $fh, '<', $SELF or fail("cannot read $SELF");
    my $in = 0;
    while (my $l = <$fh>) {
        $l =~ s/\r?\n\z//;
        if ($in) { push @data, $l if $l ne ''; next; }
        $in = 1 if $l eq '__NOLF_INSTALL_DATA__';
    }
    close $fh;
    fail('the file list is missing from the script') unless @data;
}

# This folder: the release unpacked, and nothing else? SHA256SUMS (the
# archive's last file) lists every file the archive holds and every file
# this script writes. Each of the archive's must be here as it was shipped,
# and nothing may be here that is neither, nor what the game writes as it
# runs (saves, logs, settings, screenshots) nor a disc image -- which is
# what copying the release over another installation leaves. Finder's
# .DS_Store and the ._ files macOS writes on other disks are let be too.
{
    my $sums = "$HERE/SHA256SUMS";
    -f $sums or fail('SHA256SUMS is missing: run the script in the folder the release unpacked to');
    my (%sum, %write, @bad, @extra);
    open my $fh, '<', $sums or fail("cannot read $sums");
    while (my $l = <$fh>) {
        $l =~ s/\r?\n\z//;
        $sum{substr($l, 66)} = substr($l, 0, 64) if length($l) > 66;
    }
    close $fh;
    for (@data) { $write{(split / /)[1]} = 1 if /^F /; }
    print "checking this folder ...\n";
    for my $rel (sort keys %sum) {
        next if $write{$rel};                      # the script writes it
        if (!-f "$HERE/$rel") { push @bad, "missing: $rel"; }
        elsif (sha256_file("$HERE/$rel") ne $sum{$rel}) { push @bad, "changed: $rel"; }
    }
    File::Find::find({ no_chdir => 1, wanted => sub {
        return unless -f $_ || -l $_;
        my $rel = substr($File::Find::name, length($HERE) + 1);
        return if exists $sum{$rel} || $write{$rel};
        my $leaf = basename($rel);
        return if $rel =~ m{^(SHA256SUMS|autoexec\.cfg|nolfcmds\.txt|NetHost\.txt|run-cmds\.txt|AIReactionReset\.txt)\z}i;
        return if $rel =~ m{^(Save|cd)/}i || $rel =~ /\.part\z/i;
        return if $leaf =~ /^(Thumbs\.db|desktop\.ini|\.DS_Store)\z/i || $leaf =~ /^\._/;
        return if $rel !~ m{/} && $rel =~ /\.(log|bmp|iso|bin|cue|zip)\z/i;   # top level only
        push @extra, $rel;
    } }, $HERE);
    if (@bad || @extra) {
        print STDERR "\n$ME: this folder is not just this release, unpacked:\n";
        print STDERR "  $_\n" for @bad;
        my @shown = sort @extra;
        print STDERR "  not part of it: $_\n" for @shown[0 .. ($#shown < 19 ? $#shown : 19)];
        print STDERR '  ... and ' . (@shown - 20) . " more files not part of it\n" if @shown > 20;
        print STDERR "\nThe release is the whole game, not a patch: do not copy it over another\n",
                     "installation, or over an older release. Unpack it into a new, empty\n",
                     "folder and run mac-install.command there. (Your saved games are in the\n",
                     "old folder's Save folder: copy it across afterwards.)\n";
        exit 1;
    }
}

# The discs: their sizes and checksums, from the list.
my (%dsize, %dsha, %dname, %draw, %disc);
for (@data) {
    if (/^D (\S+) (\d+) (\S+) (.*)$/) { ($dsize{$1}, $dsha{$1}, $dname{$1}) = ($2, $3, $4); }
    elsif (/^R (\S+) (\d+)$/) { $draw{$1} = $2; }
}

# A .zip's members from its central directory: (name, method, packed size,
# size, offset of the local header) of each. Not zip64: a CD image fits.
sub zip_members {
    my ($path) = @_;
    open my $fh, '<:raw', $path or return ();
    my $len = -s $fh;
    my $tail = $len < 65557 ? $len : 65557;
    sysseek($fh, $len - $tail, 0);
    sysread($fh, my $buf, $tail);
    my $at = rindex($buf, "PK\x05\x06");
    return () if $at < 0 || length($buf) - $at < 22;
    my ($count, $cdsize, $cdoff) = unpack('x10 v V V', substr($buf, $at, 22));
    sysseek($fh, $cdoff, 0);
    sysread($fh, my $cd, $cdsize) == $cdsize or return ();
    my (@m, $p);
    $p = 0;
    for (1 .. $count) {
        last if substr($cd, $p, 4) ne "PK\x01\x02";
        my ($method, $packed, $size, $nlen, $xlen, $clen, $local) =
            unpack('x10 v x8 V V v v v x8 V', substr($cd, $p, 46));
        push @m, [substr($cd, $p + 46, $nlen), $method, $packed, $size, $local];
        $p += 46 + $nlen + $xlen + $clen;
    }
    close $fh;
    return @m;
}

# The one .bin of a .zip, or undef (then the .zip is not a disc).
sub zip_bin {
    my @bins = grep { $_->[0] =~ /\.bin\z/i } zip_members($_[0]);
    return @bins == 1 ? $bins[0] : undef;
}

# A raw image's (2352-byte sector) size: a .bin's, or a .zip's .bin's.
sub raw_size {
    my ($f) = @_;
    if ($f =~ /\.zip\z/i) { my $b = zip_bin($f); return $b ? $b->[3] : 0; }
    return -s $f;
}

# A reader of a raw image's bytes: a code ref giving up to n bytes
# (fewer only at the end).
sub raw_reader {
    my ($f) = @_;
    open my $fh, '<:raw', $f or fail("$f: cannot read it");
    if ($f !~ /\.zip\z/i) {
        return sub { my $n = sysread($fh, my $b, $_[0]); defined $n or fail("$f: a read error"); $b };
    }
    my $m = zip_bin($f) or fail("$f: not one .bin in it");
    my ($name, $method, $packed, undef, $local) = @$m;
    sysseek($fh, $local, 0);
    my $h;
    sysread($fh, $h, 30) == 30 && substr($h, 0, 4) eq "PK\x03\x04" or fail("$f: a damaged .zip");
    my ($nlen, $xlen) = unpack('x26 v v', $h);
    sysseek($fh, $local + 30 + $nlen + $xlen, 0);
    my $left = $packed;
    my $inf;
    if ($method == 8) {
        ($inf) = Compress::Raw::Zlib::Inflate->new(-WindowBits => -MAX_WBITS, -Bufsize => 1 << 20);
        $inf or fail("$f: cannot inflate it");
    } elsif ($method != 0) {
        fail("$f: $name is compressed in a way this script cannot read (method $method)");
    }
    my $out = '';
    return sub {
        my ($want) = @_;
        while (length($out) < $want && $left > 0) {
            my $n = sysread($fh, my $b, $left < (1 << 20) ? $left : (1 << 20));
            defined $n && $n > 0 or fail("$f: the .zip ended early");
            $left -= $n;
            if ($inf) {
                my $st = $inf->inflate($b, my $o);
                $st == Z_OK || $st == Z_STREAM_END or fail("$f: a damaged .zip ($st)");
                $out .= $o;
            } else { $out .= $b; }
        }
        return substr($out, 0, $want, '');
    };
}

# The .iso of a raw image: the 2048 bytes of user data of each sector
# (MODE1: after the 16-byte sync and header).
sub convert {
    my ($f, $to) = @_;
    my $read = raw_reader($f);
    open my $out, '>:raw', $to or fail("$to: cannot write it");
    my $S = 2352;
    while (1) {
        my $buf = $read->($S * 512);
        last if $buf eq '';
        length($buf) % $S == 0 or fail(basename($f) . ': not whole 2352-byte sectors');
        my $o = '';
        for (my $i = 0; $i < length($buf); $i += $S) { $o .= substr($buf, $i + 16, 2048); }
        print $out $o or fail("$to: cannot write it (is the disk full?)");
    }
    close $out or fail("$to: cannot write it (is the disk full?)");
}

# take(image, label): which disc an .iso is, if any (by size, then SHA-256).
my $found = 0;
sub take {
    my ($f, $label) = @_;
    my $size = -s $f;
    for my $n (sort keys %dsize) {
        next if $disc{$n} || $size != $dsize{$n};
        print "checking $label ...\n";
        if (sha256_file($f) eq $dsha{$n}) {
            $disc{$n} = $f;
            $found++;
            print "  disc $n\n";
            return 1;
        }
        print "  the size of disc $n, but not its contents (a bad copy?)\n";
    }
    return 0;
}

my @candidates = @ARGV;
if (!@candidates) {
    # .iso first: one is used as it is, the others need converting.
    for my $ext ('iso', 'bin', 'zip') {
        for my $dir ($HERE, dirname($HERE)) {
            opendir(my $dh, $dir) or next;
            push @candidates, map { "$dir/$_" } sort grep { /\.\Q$ext\E\z/i && -f "$dir/$_" } readdir $dh;
            closedir $dh;
        }
    }
}
for my $f (@candidates) {
    last if $found >= keys %dsize;                     # both found
    -f $f or fail("$f: no such file");
    my $label = basename($f);
    if ($f =~ /\.cue\z/i) {
        open my $fh, '<', $f or fail("$f: cannot read it");
        my $cue = do { local $/; <$fh> };
        close $fh;
        my ($bin) = $cue =~ /^\s*FILE\s+"([^"]+)"/m;
        if (!defined $bin || $cue !~ /TRACK\s+01\s+MODE1\/2352/i) {
            print "  $label: not a one-track MODE1/2352 cue sheet; skipped\n";
            next;
        }
        $f = dirname($f) . "/$bin";
        -f $f or fail("$label names $bin, which is not beside it");
    }
    if ($f =~ /\.(bin|zip)\z/i) {
        my $size = raw_size($f);
        for my $n (sort keys %draw) {
            next if $disc{$n} || $size != $draw{$n};
            my $tmp = "$HERE/.disc$n.iso.part";
            push @TEMPS, $tmp;
            print "converting $label to an .iso ...\n";
            convert($f, $tmp);
            take($tmp, "$label (converted)") or unlink $tmp;
            last;
        }
    } else {
        take($f, $label);
    }
}
my $missing = 0;
for my $n (sort keys %dsize) {
    next if $disc{$n};
    print STDERR "$ME: no image of disc $n: $dname{$n}\n",
                 "  (.iso: $dsize{$n} bytes, SHA-256 $dsha{$n}; or a raw .bin/.cue, or a .zip of one)\n";
    $missing = 1;
}
if ($missing) {
    print STDERR "\nPut both disc images in this folder and run it again, or give them to it\n",
                 "in Terminal: ./mac-install.command disc1.zip disc2.zip\n";
    exit 1;
}

# The files.
sub ok {   # ok(path, size, sha): is the file there, and right?
    my ($p, $size, $sha) = @_;
    return -f $p && -s $p == $size && sha256_file($p) eq $sha;
}
my %stream;
for my $n (keys %disc) { open($stream{$n}, '<:raw', $disc{$n}) or fail("$disc{$n}: cannot read it"); }
my ($written, $kept, $skip, $out, $name, $dest, $part, $size, $sha) = (0, 0, 1);
for (@data) {
    my @p = split / /;
    my $tag = $p[0];
    if ($tag eq 'F') {
        ($name, $size, $sha) = @p[1 .. 3];
        $dest = "$HERE/$name";
        $part = "$dest.part";
        $skip = ($p[4] eq 'once' && -e $dest) || ok($dest, $size, $sha);
        if ($skip) { $kept++; next; }
        print "$name\n";
        make_path(dirname($dest));
        open($out, '>:raw', $part) or fail("$part: cannot write it");
    } elsif ($tag eq 'C') {
        next if $skip;
        my ($n, $off, $left) = @p[1 .. 3];
        my $in = $stream{$n};
        sysseek($in, $off, 0) or fail("disc $n: cannot seek");
        while ($left > 0) {
            my $got = sysread($in, my $b, $left < (1 << 20) ? $left : (1 << 20));
            defined $got && $got > 0 or fail("disc $n ended early");
            print $out $b or fail("$part: cannot write it (is the disk full?)");
            $left -= $got;
        }
    } elsif ($tag eq 'Z') {
        next if $skip;
        my $left = $p[1];
        while ($left > 0) {
            my $n = $left < (1 << 20) ? $left : (1 << 20);
            print $out "\0" x $n or fail("$part: cannot write it (is the disk full?)");
            $left -= $n;
        }
    } elsif ($tag eq 'L') {
        next if $skip;
        print $out decode_base64($p[1]) or fail("$part: cannot write it (is the disk full?)");
    } elsif ($tag eq 'E') {
        next if $skip;
        close $out or fail("$part: cannot write it (is the disk full?)");
        if (!ok($part, $size, $sha)) {
            unlink $part;
            fail("$name came out wrong (a disk error?); the files before it are in place");
        }
        rename($part, $dest) or fail("$dest: cannot move it into place");
        $written++;
    }
}
close $_ for values %stream;
make_path("$HERE/Save");
print "\ndone: $written files written, $kept already in place.\n",
      "Play by running NOLF.exe in CrossOver (or another Wine): docs/INSTALL.md, \"Mac\".\n";
exit 0;
__END__
