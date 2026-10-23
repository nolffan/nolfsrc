# windows-install.ps1 -- the PowerShell inside windows-install.bat
# (build/mkinstall.py puts it there; the .bat runs it with -File, passing its
# own path and the disc images dropped on it). Windows PowerShell 5.1; ASCII
# only, as it reads a .ps1 without a byte-order mark as the ANSI code page.
#
# What linux-install.sh does: check that the folder is the release unpacked
# and nothing else (SHA256SUMS), find the two disc images (the arguments, or
# the images in the game folder or the one above it; a redump .bin, .cue or
# .zip converted to a temporary .iso first), check their sizes and SHA-256,
# then write each file of the list at the end of the .bat from
# its byte ranges -- C <disc> <offset> <length>, Z <length> zeros, L <base64>
# -- check its size and SHA-256, and move it into place.
param(
    [Parameter(Mandatory = $true)][string]$Self,
    [Parameter(ValueFromRemainingArguments = $true)][string[]]$Images
)
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $Self

# Converted images (.disc<n>.iso.part), removed however the script ends.
$script:temps = @()
function Remove-Temps {
    foreach ($t in $script:temps) { Remove-Item -LiteralPath $t -Force -ErrorAction SilentlyContinue }
}

# Any other error: say it, and still remove the converted images.
trap {
    Write-Host ''
    Write-Host "windows-install: $_" -ForegroundColor Red
    if ($out) { $out.Close() }
    if ($streams) { foreach ($s in $streams.Values) { $s.Close() } }
    Remove-Temps
    exit 1
}

function Fail($msg) {
    Write-Host ''
    Write-Host "windows-install: $msg" -ForegroundColor Red
    Remove-Temps
    exit 1
}

function Get-Sha256($path) {
    return (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
}

# The list: every line after the marker line.
$text = [IO.File]::ReadAllText($Self)
$marker = '__NOLF_INSTALL' + '_DATA__'
$at = $text.IndexOf("`n" + $marker)
if ($at -lt 0) { Fail 'the file list is missing from the .bat' }
$lines = $text.Substring($at + 1 + $marker.Length) -split "`r?`n" | Where-Object { $_ -ne '' }

# This folder: the release unpacked, and nothing else? (linux-install.sh's
# check_dir.) SHA256SUMS lists every file the archive holds and every file
# this script writes. Each of the archive's must be here as it was shipped,
# and nothing may be here that is neither, nor what the game writes as it
# runs (saves, logs, settings, screenshots) nor a disc image. Names compare
# without case, as Windows does.
$sumsPath = Join-Path $here 'SHA256SUMS'
if (-not (Test-Path -LiteralPath $sumsPath -PathType Leaf)) {
    Fail 'SHA256SUMS is missing: run windows-install.bat in the folder the release unpacked to'
}
$sums = @{}
foreach ($l in [IO.File]::ReadAllLines($sumsPath)) {
    if ($l.Length -gt 66) { $sums[$l.Substring(66)] = $l.Substring(0, 64) }
}
$writes = @{}
foreach ($l in $lines) {
    if ($l.StartsWith('F ')) { $writes[$l.Split(' ')[1]] = $true }
}
Write-Host 'checking this folder ...'
$bad = @()
foreach ($rel in $sums.Keys) {
    if ($writes.ContainsKey($rel)) { continue }          # this script writes it
    $path = Join-Path $here ($rel.Replace('/', [IO.Path]::DirectorySeparatorChar))
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { $bad += "missing: $rel" }
    elseif ((Get-Sha256 $path) -ne $sums[$rel]) { $bad += "changed: $rel" }
}
$extra = @()
foreach ($f in Get-ChildItem -LiteralPath $here -Recurse -File -Force) {
    $rel = $f.FullName.Substring($here.TrimEnd('\').Length + 1).Replace('\', '/')
    if ($sums.ContainsKey($rel) -or $writes.ContainsKey($rel)) { continue }
    $leaf = $f.Name
    if (@('SHA256SUMS', 'autoexec.cfg', 'nolfcmds.txt', 'NetHost.txt', 'run-cmds.txt',
          'AIReactionReset.txt') -contains $rel) { continue }
    if ($rel -like 'Save/*' -or $rel -like 'Mods/*' -or $rel -like 'cd/*' -or $rel -like '*.part') { continue }
    if (@('Thumbs.db', 'desktop.ini', '.DS_Store') -contains $leaf) { continue }
    if (-not $rel.Contains('/') -and
        @('*.log', '*.bmp', '*.iso', '*.bin', '*.cue', '*.zip' | Where-Object { $rel -like $_ }).Count) { continue }
    $extra += $rel
}
if ($bad.Count -or $extra.Count) {
    Write-Host ''
    Write-Host 'windows-install: this folder is not just this release, unpacked:' -ForegroundColor Red
    foreach ($b in $bad) { Write-Host "  $b" }
    foreach ($e in ($extra | Select-Object -First 20)) { Write-Host "  not part of it: $e" }
    if ($extra.Count -gt 20) { Write-Host "  ... and $($extra.Count - 20) more files not part of it" }
    Write-Host ''
    Write-Host 'The release is the whole game, not a patch: do not copy it over another'
    Write-Host 'installation, or over an older release. Unpack it into a new, empty'
    Write-Host 'folder and run windows-install.bat there. (Your saved games are in the'
    Write-Host 'old folder''s Save folder: copy it across afterwards.)'
    exit 1
}

$discs = @{}
foreach ($l in $lines) {
    if ($l.StartsWith('D ')) {
        $p = $l.Split(' ', 5)
        $discs[$p[1]] = @{ Size = [int64]$p[2]; Sha = $p[3]; Name = $p[4]; Raw = [int64]0; Path = $null }
    }
}
foreach ($l in $lines) {
    if ($l.StartsWith('R ')) { $p = $l.Split(' '); $discs[$p[1]].Raw = [int64]$p[2] }
}

# A redump image: a .bin, or the one .bin of a .zip, of 2352-byte MODE1
# sectors. Open-Raw gives @(size, stream) (stream $null with -SizeOnly);
# a .zip without exactly one .bin has size 0, so it matches no disc.
Add-Type -AssemblyName System.IO.Compression.FileSystem
function Open-Raw($path, [switch]$SizeOnly) {
    if ($path.ToLowerInvariant().EndsWith('.zip')) {
        try { $zip = [IO.Compression.ZipFile]::OpenRead($path) } catch { return @([int64]0, $null) }
        $bins = @($zip.Entries | Where-Object { $_.FullName.ToLowerInvariant().EndsWith('.bin') })
        if ($bins.Count -ne 1) { $zip.Dispose(); return @([int64]0, $null) }
        if ($SizeOnly) { $size = $bins[0].Length; $zip.Dispose(); return @($size, $null) }
        return @($bins[0].Length, $bins[0].Open())
    }
    $size = ([IO.FileInfo]$path).Length
    if ($SizeOnly) { return @($size, $null) }
    return @($size, [IO.File]::OpenRead($path))
}

# The .iso of a redump image: the 2048 bytes of user data of each sector,
# after its 16-byte sync and header.
function Convert-Raw($path, $out) {
    $r = Open-Raw $path
    $src = $r[1]
    $dst = [IO.File]::Create($out)
    try {
        $S = 2352
        $buf = New-Object byte[] ($S * 512)
        while ($true) {
            $have = 0
            while ($have -lt $buf.Length) {       # a zip stream can read short
                $n = $src.Read($buf, $have, $buf.Length - $have)
                if ($n -le 0) { break }
                $have += $n
            }
            if ($have -eq 0) { break }
            if ($have % $S) { Fail "$(Split-Path -Leaf $path): not whole 2352-byte sectors" }
            for ($i = 0; $i -lt $have; $i += $S) { $dst.Write($buf, $i + 16, 2048) }
            if ($have -lt $buf.Length) { break }
        }
    } finally {
        $dst.Close()
        $src.Close()
    }
}

# Which disc an .iso is, if any: by size, then SHA-256.
function Use-Iso($path, $label) {
    $size = ([IO.FileInfo]$path).Length
    foreach ($n in @($discs.Keys)) {
        $d = $discs[$n]
        if ($d.Path -or $size -ne $d.Size) { continue }
        Write-Host "checking $label ..."
        if ((Get-Sha256 $path) -eq $d.Sha) {
            $d.Path = $path
            Write-Host "  disc $n"
            return $true
        }
        Write-Host "  the size of disc $n, but not its contents (a bad copy?)"
    }
    return $false
}

if (-not $Images -or $Images.Count -eq 0) {
    # .iso first: one is used as it is, the others need converting.
    $Images = @()
    $up = Split-Path -Parent $here
    foreach ($pat in '*.iso', '*.bin', '*.zip') {
        foreach ($dir in @($here, $up)) {
            if ($dir) { $Images += @(Get-ChildItem -LiteralPath $dir -Filter $pat -File | ForEach-Object { $_.FullName }) }
        }
    }
}
foreach ($img in $Images) {
    if (-not @($discs.Values | Where-Object { -not $_.Path })) { break }   # both found
    if (-not (Test-Path -LiteralPath $img -PathType Leaf)) { Fail "$img`: no such file" }
    $label = Split-Path -Leaf $img
    $lower = $img.ToLowerInvariant()
    if ($lower.EndsWith('.cue')) {
        $cue = [IO.File]::ReadAllText($img)
        $m = [regex]::Match($cue, '(?m)^\s*FILE\s+"([^"]+)"')
        if (-not $m.Success -or $cue -notmatch '(?i)TRACK\s+01\s+MODE1/2352') {
            Write-Host "  $label`: not a one-track MODE1/2352 cue sheet; skipped"
            continue
        }
        $img = Join-Path (Split-Path -Parent $img) $m.Groups[1].Value
        if (-not (Test-Path -LiteralPath $img -PathType Leaf)) { Fail "$label names $($m.Groups[1].Value), which is not beside it" }
        $lower = '.bin'
    }
    if ($lower.EndsWith('.bin') -or $lower.EndsWith('.zip')) {
        $size = (Open-Raw $img -SizeOnly)[0]
        foreach ($n in @($discs.Keys)) {
            $d = $discs[$n]
            if ($d.Path -or $size -ne $d.Raw) { continue }
            $tmp = Join-Path $here ".disc$n.iso.part"
            $script:temps += $tmp
            Write-Host "converting $label to an .iso ..."
            Convert-Raw $img $tmp
            if (-not (Use-Iso $tmp "$label (converted)")) { Remove-Item -LiteralPath $tmp -Force }
            break
        }
    } else {
        [void](Use-Iso $img $label)
    }
}
$missing = $false
foreach ($n in ($discs.Keys | Sort-Object)) {
    $d = $discs[$n]
    if (-not $d.Path) {
        Write-Host "no image of disc $n`: $($d.Name)" -ForegroundColor Red
        Write-Host "  (.iso: $($d.Size) bytes, SHA-256 $($d.Sha); or redump's .bin/.cue or .zip)"
        $missing = $true
    }
}
if ($missing) { Fail 'drop both disc images on windows-install.bat, or put them in this folder' }

function Test-Done($path, $size, $sha) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { return $false }
    if (([IO.FileInfo]$path).Length -ne $size) { return $false }
    return (Get-Sha256 $path) -eq $sha
}

$streams = @{}
foreach ($n in $discs.Keys) { $streams[$n] = [IO.File]::OpenRead($discs[$n].Path) }
$buf = New-Object byte[] (1MB)
$zero = New-Object byte[] (1MB)
$out = $null
$written = 0
$kept = 0
try {
    foreach ($l in $lines) {
        $p = $l.Split(' ')
        switch ($p[0]) {
            'F' {
                $name = $p[1]
                $dest = Join-Path $here ($name.Replace('/', [IO.Path]::DirectorySeparatorChar))
                $size = [int64]$p[2]
                $sha = $p[3]
                $part = "$dest.part"
                $skip = (($p[4] -eq 'once') -and (Test-Path -LiteralPath $dest)) -or (Test-Done $dest $size $sha)
                if ($skip) { $kept++ } else {
                    Write-Host $name
                    $dir = Split-Path -Parent $dest
                    if (-not (Test-Path -LiteralPath $dir)) { New-Item -ItemType Directory -Path $dir | Out-Null }
                    $out = [IO.File]::Create($part)
                }
            }
            'C' {
                if ($skip) { continue }
                $src = $streams[$p[1]]
                $src.Position = [int64]$p[2]
                $left = [int64]$p[3]
                while ($left -gt 0) {
                    $n = $src.Read($buf, 0, [int][Math]::Min($left, [int64]$buf.Length))
                    if ($n -le 0) { Fail "disc $($p[1]) ended early" }
                    $out.Write($buf, 0, $n)
                    $left -= $n
                }
            }
            'Z' {
                if ($skip) { continue }
                $left = [int64]$p[1]
                while ($left -gt 0) {
                    $n = [int][Math]::Min($left, [int64]$zero.Length)
                    $out.Write($zero, 0, $n)
                    $left -= $n
                }
            }
            'L' {
                if ($skip) { continue }
                $b = [Convert]::FromBase64String($p[1])
                $out.Write($b, 0, $b.Length)
            }
            'E' {
                if ($skip) { continue }
                $out.Close()
                $out = $null
                if (-not (Test-Done $part $size $sha)) {
                    Remove-Item -LiteralPath $part -Force
                    Fail "$name came out wrong (a disk error?); the files before it are in place"
                }
                Move-Item -LiteralPath $part -Destination $dest -Force
                $written++
            }
        }
    }
} finally {
    if ($out) { $out.Close() }
    foreach ($s in $streams.Values) { $s.Close() }
    Remove-Temps                    # once nothing has them open
}
$save = Join-Path $here 'Save'
if (-not (Test-Path -LiteralPath $save)) { New-Item -ItemType Directory -Path $save | Out-Null }
Write-Host ''
Write-Host "done: $written files written, $kept already in place. Start the game with NOLF.exe" -ForegroundColor Green
exit 0
