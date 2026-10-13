<#
.SYNOPSIS
    Build all my12 release images: normal (PCB 22/11) and LIGHTNINGBOARD1 (PCB 21), banks A and I.

.DESCRIPTION
    Produces, for the BUILD number in Core/Inc/version.h:
        my12-A22-<build>.bin  my12-A11-<build>.bin   (same image, bank A, 0x08000000)
        my12-I22-<build>.bin  my12-I11-<build>.bin   (same image, bank I, 0x08100000)
        my12-A21-<build>.bin  my12-I21-<build>.bin   (LIGHTNINGBOARD1_BUILD on, detector 1)
    and prints the size, reset vector and CRC (server crc1 = A, crc2 = I) of each.

    It temporarily edits two tracked files - the LIGHTNINGBOARD1_BUILD switch in Core/Inc/version.h and the
    FLASH origin in STM32F767ZITX_FLASH.ld - and puts back their exact original bytes when it finishes
    (also on failure), so uncommitted edits to them survive. Every image is built after `make clean`
    (restored files keep old mtimes otherwise and make would not relink).

    The last build is the normal bank-A image, so Debug/ is left in the normal configuration.
    Run from PowerShell (not Git Bash: its make mangles TMP/TEMP for the native toolchain).

.PARAMETER OutDir
    Where the .bin files go. Default: the project root.

.PARAMETER ToolchainBin
    arm-none-eabi-gcc bin directory. Default: newest STM32CubeIDE gnu-tools under C:\ST, else whatever is on PATH.

.PARAMETER SkipLB1
    Build only the normal 22/11 images.

.EXAMPLE
    .\tools\build_release.ps1
#>
[CmdletBinding()]
param(
    [string]$OutDir,
    [string]$ToolchainBin,
    [switch]$SkipLB1
)
$ErrorActionPreference = 'Stop'

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$dbg  = Join-Path $root 'Debug'
$ver  = Join-Path $root 'Core\Inc\version.h'
if (-not $OutDir) { $OutDir = $root }
New-Item -ItemType Directory -Force $OutDir | Out-Null
$OutDir = (Resolve-Path $OutDir).Path

# the linker script the generated makefile actually uses (-T"...")
$mk = Get-Content (Join-Path $dbg 'makefile') -Raw
if ($mk -notmatch '-T"([^"]+\.ld)"') { throw 'cannot find the -T linker script in Debug/makefile' }
$ld = $Matches[1]
if (-not (Test-Path $ld)) { throw "linker script not found: $ld" }
if ((Resolve-Path $ld).Path -ne (Join-Path $root 'STM32F767ZITX_FLASH.ld')) {
    Write-Warning "makefile links with $ld (not this checkout's STM32F767ZITX_FLASH.ld) - that file is the one edited"
}

# toolchain
if (-not $ToolchainBin) {
    $ToolchainBin = Get-ChildItem 'C:\ST\STM32CubeIDE*\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32*\tools\bin' -Directory -ErrorAction SilentlyContinue |
        Sort-Object FullName | Select-Object -Last 1 -ExpandProperty FullName
}
if ($ToolchainBin) { $env:PATH = "$ToolchainBin;$env:PATH" }
if (Test-Path 'C:\msys64\usr\bin') { $env:PATH = "C:\msys64\usr\bin;$env:PATH" }
foreach ($tool in 'arm-none-eabi-gcc', 'make') {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) { throw "$tool not found (use -ToolchainBin)" }
}

# build number
$verText = [IO.File]::ReadAllText($ver)
if ($verText -notmatch '(?m)^#define BUILD\s+(\d+)') { throw 'BUILD not found in version.h' }
$build = $Matches[1]

if (-not ('Xcrc' -as [type])) {
    Add-Type -TypeDefinition @'
public static class Xcrc {
    // server/bootloader CRC: poly 0x04C11DB7, MSB first, init 0xFFFFFFFF, no final xor
    public static uint Calc(byte[] b) {
        uint c = 0xFFFFFFFF;
        foreach (byte x in b) {
            c ^= (uint)x << 24;
            for (int i = 0; i < 8; i++) c = ((c & 0x80000000) != 0) ? (c << 1) ^ 0x04C11DB7 : (c << 1);
        }
        return c;
    }
}
'@
}

$utf8 = New-Object System.Text.UTF8Encoding($false)
$origVer = [IO.File]::ReadAllBytes($ver)
$origLd  = [IO.File]::ReadAllBytes($ld)

function Set-Bank([string]$bank) {      # 'A' = 0x08000000, 'I' = 0x08100000 (the active, uncommented FLASH line)
    $origin = if ($bank -eq 'A') { '0x8000000' } else { '0x8100000' }
    $rx = '(?m)^([ \t]*FLASH[ \t]+\(rx\)[ \t]*:[ \t]*ORIGIN = )0x8[01]00000(,)'
    $t = [IO.File]::ReadAllText($ld)
    if ([regex]::Matches($t, $rx).Count -ne 1) { throw 'linker script: expected exactly one active FLASH ORIGIN line' }
    [IO.File]::WriteAllText($ld, [regex]::Replace($t, $rx, "`${1}$origin`${2}"), $utf8)
}
function Set-LB1([bool]$on) {
    $rx = '#if [01](\r?\n#define LIGHTNINGBOARD1_BUILD)'
    $t = [IO.File]::ReadAllText($ver)
    if (-not [regex]::IsMatch($t, $rx)) { throw 'version.h: LIGHTNINGBOARD1_BUILD switch not found' }
    $new = if ($on) { '#if 1$1' } else { '#if 0$1' }
    [IO.File]::WriteAllText($ver, [regex]::Replace($t, $rx, $new), $utf8)
}

$results = @{}
function Build-Image([string]$tag, [string]$bank, [bool]$lb1) {
    Write-Output ("=== {0}: bank {1}, LIGHTNINGBOARD1_BUILD={2}  {3} ===" -f $tag, $bank, $lb1, (Get-Date -Format HH:mm:ss))
    Set-Bank $bank
    Set-LB1 $lb1
    Push-Location $dbg
    try {
        (make -k clean 2>&1) | Out-Null
        $out = make -k all 2>&1
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path 'my12.bin')) { $out | Select-Object -Last 25; throw "build $tag failed" }
        $out | Select-String -Pattern ': error' | Select-Object -First 5
        $bin = [IO.File]::ReadAllBytes((Join-Path $dbg 'my12.bin'))
        $list = Get-Content (Join-Path $dbg 'my12.list')
    } finally { Pop-Location }

    $reset  = [BitConverter]::ToUInt32($bin, 4)
    $crc    = [Xcrc]::Calc($bin)
    $banner = [Text.Encoding]::ASCII.GetString($bin).Contains('LIGHTNINGBOARD1 BUILD')
    $gi     = ($list | Select-String -Pattern '<getboardpcb>:$' | Select-Object -First 1).LineNumber
    $pcb    = ($list[$gi..($gi + 30)] | Select-String -Pattern 'moveq\s+r2, #(\d+)' | Select-Object -First 1).Matches.Groups[1].Value
    $bldRef = @($list | Select-String -Pattern "#$build\b").Count

    # sanity: right bank, right board constant, build number present
    $wantBank = if ($bank -eq 'A') { 0x08000000 } else { 0x08100000 }
    $wantPcb  = if ($lb1) { '21' } else { '22' }
    if (($reset -band 0xFFF00000) -ne $wantBank) { throw ("{0}: reset vector 0x{1:x8} is not in bank {2}" -f $tag, $reset, $bank) }
    if ($pcb -ne $wantPcb)       { throw "$tag`: getboardpcb assigns #$pcb, expected #$wantPcb" }
    if ($banner -ne $lb1)        { throw "$tag`: LB1 banner string present=$banner, expected $lb1" }
    if ($bldRef -eq 0)           { throw "$tag`: build number $build not found in the image" }

    $results[$tag] = [pscustomobject]@{ Bytes = $bin; Reset = $reset; Crc = $crc }
    Write-Output ("{0}: {1} bytes, reset 0x{2:x8}, crc 0x{3:x8}, low-PC0 PCB #{4}, LB1 banner {5}" -f $tag, $bin.Length, $reset, $crc, $pcb, $banner)
}

Push-Location $root
$head  = (git rev-parse --short HEAD) 2>$null
$dirty = @(git status --short -- Core STM32F767ZITX_FLASH.ld 2>$null)
Pop-Location
Write-Output "my12 build $build, git HEAD $head, Core/ and linker script dirty: $($dirty.Count -gt 0)"
if ($dirty.Count -gt 0) { $dirty | ForEach-Object { Write-Output "   $_" } }

try {
    Build-Image 'I22' 'I' $false
    if (-not $SkipLB1) {
        Build-Image 'I21' 'I' $true
        Build-Image 'A21' 'A' $true
    }
    Build-Image 'A22' 'A' $false         # last: leaves Debug/ in the normal bank-A configuration
}
finally {
    [IO.File]::WriteAllBytes($ver, $origVer)
    [IO.File]::WriteAllBytes($ld,  $origLd)
}

# name the files the way the server expects (22 and 11 are the same image)
$files = [ordered]@{ "A22" = 'A22'; "A11" = 'A22'; "I22" = 'I22'; "I11" = 'I22' }
if (-not $SkipLB1) { $files["A21"] = 'A21'; $files["I21"] = 'I21' }
Write-Output ""
Write-Output "=== files in $OutDir ==="
foreach ($n in $files.Keys) {
    $r = $results[$files[$n]]
    $path = Join-Path $OutDir "my12-$n-$build.bin"
    [IO.File]::WriteAllBytes($path, $r.Bytes)
    Write-Output ("my12-{0}-{1}.bin  {2} bytes  crc 0x{3:x8}  ({4})" -f $n, $build, $r.Bytes.Length, $r.Crc, $(if ($n.StartsWith('A')) { 'crc1' } else { 'crc2' }))
}
Write-Output ""
Write-Output "version.h and the linker script are restored; Debug/ holds the normal bank-A build."
