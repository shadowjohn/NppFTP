param(
    [ValidateSet("x64")]
    [string] $Arch = "x64",

    [ValidateSet("Release", "Debug")]
    [string] $Config = "Release",

    [switch] $CheckOnly,
    [switch] $ConfigureOnly
)

$ErrorActionPreference = "Stop"

if ($PSVersionTable.PSVersion -lt [version]"7.6") {
    throw "PowerShell 7.6+ is recommended; found $($PSVersionTable.PSVersion)."
}

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $Root "_build"
$ToolsDir = Join-Path $BuildDir "tools"

$StrawberryPerlVersion = "5.42.2.1"
$StrawberryPerlZipName = "strawberry-perl-$StrawberryPerlVersion-64bit-portable.zip"
$StrawberryPerlUrl = "https://github.com/StrawberryPerl/Perl-Dist-Strawberry/releases/download/SP_54221_64bit/$StrawberryPerlZipName"
$StrawberryPerlSha256 = "32d83be90cf04b807cfb9477482bc36302cdee6f5b04cf57e81adecbd8f07898"

function Test-PerlForOpenSSL {
    param(
        [string] $PerlExe,
        [string] $PathPrefix
    )

    $oldPath = $env:PATH
    try {
        if ($PathPrefix) {
            $env:PATH = "$PathPrefix;$oldPath"
        }
        & $PerlExe -MLocale::Maketext::Simple -MIPC::Cmd -e "print qq(ok\n)" > $null 2>&1
        return $LASTEXITCODE -eq 0
    } finally {
        $env:PATH = $oldPath
    }
}

function Get-UsablePerl {
    $candidates = @()
    $pathPerl = Get-Command perl -ErrorAction SilentlyContinue
    if ($pathPerl) {
        $dir = Split-Path -Parent $pathPerl.Source
        $candidates += @{ Exe = $pathPerl.Source; Path = $dir }
    }

    foreach ($exe in @(
        "C:\Strawberry\perl\bin\perl.exe",
        "$env:ProgramFiles\Strawberry Perl\perl\bin\perl.exe",
        "$env:ProgramFiles\Git\usr\bin\perl.exe"
    )) {
        if (Test-Path -LiteralPath $exe) {
            $dir = Split-Path -Parent $exe
            $candidates += @{ Exe = $exe; Path = $dir }
        }
    }

    foreach ($candidate in $candidates) {
        if (Test-PerlForOpenSSL $candidate.Exe $candidate.Path) {
            return $candidate
        }
    }

    New-Item -ItemType Directory -Force -Path $ToolsDir | Out-Null
    $zip = Join-Path $ToolsDir $StrawberryPerlZipName
    if (-not (Test-Path -LiteralPath $zip) -or (Get-FileHash -Algorithm SHA256 -LiteralPath $zip).Hash.ToLowerInvariant() -ne $StrawberryPerlSha256) {
        Write-Host "Downloading Strawberry Perl portable $StrawberryPerlVersion..."
        $ProgressPreference = "SilentlyContinue"
        Invoke-WebRequest -Uri $StrawberryPerlUrl -OutFile $zip
    }

    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $zip).Hash.ToLowerInvariant()
    if ($hash -ne $StrawberryPerlSha256) {
        Remove-Item -LiteralPath $zip -Force
        throw "Strawberry Perl checksum mismatch: $hash"
    }

    $extractDir = Join-Path $ToolsDir "strawberry-perl-$StrawberryPerlVersion"
    $perlExe = Join-Path $extractDir "perl\bin\perl.exe"
    if (-not (Test-Path -LiteralPath $perlExe)) {
        Remove-Item -LiteralPath $extractDir -Recurse -Force -ErrorAction SilentlyContinue
        Expand-Archive -LiteralPath $zip -DestinationPath $extractDir
    }

    $pathPrefix = Join-Path $extractDir "perl\bin"

    if (-not (Test-PerlForOpenSSL $perlExe $pathPrefix)) {
        throw "Bootstrapped Strawberry Perl is not usable for OpenSSL Configure."
    }

    return @{ Exe = $perlExe; Path = $pathPrefix }
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw "vswhere.exe was not found. Install Visual Studio with C++ tools."
}

$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) {
    throw "Visual Studio C++ tools were not found."
}

$version = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion
$major = [int]($version.Split(".")[0])
$generator = switch ($major) {
    { $_ -ge 18 } { "Visual Studio 18 2026"; break }
    17 { "Visual Studio 17 2022"; break }
    16 { "Visual Studio 16 2019"; break }
    default { throw "Unsupported Visual Studio version: $version" }
}

$vsDevCmd = Join-Path $vsPath "Common7\Tools\VsDevCmd.bat"
if (-not (Test-Path -LiteralPath $vsDevCmd)) {
    throw "VsDevCmd.bat was not found under $vsPath."
}

$perl = Get-UsablePerl

Write-Host "Build environment"
Write-Host "  Visual Studio: $vsPath"
Write-Host "  Version:       $version"
Write-Host "  Generator:     $generator"
Write-Host "  Perl:          $($perl.Exe)"
Write-Host "  Build dir:     $BuildDir"
Write-Host "  Arch/config:   $Arch $Config"

$steps = @(
    "call `"$vsDevCmd`" -arch=x64",
    "set `"PATH=$($perl.Path);!PATH!`"",
    "set `"LC_ALL=`"",
    "set `"LC_CTYPE=`"",
    "set `"LANG=`"",
    "where cmake",
    "cmake --version",
    "where cl",
    "where nmake",
    "where perl",
    "perl -v"
)

if (-not $CheckOnly) {
    $steps += "cmake -S `"$Root`" -B `"$BuildDir`" -G `"$generator`" -A $Arch"
}

if (-not $CheckOnly -and -not $ConfigureOnly) {
    $steps += "cmake --build `"$BuildDir`" --config $Config --target package"
}

& cmd.exe /V:ON /C ($steps -join " && ")
exit $LASTEXITCODE
