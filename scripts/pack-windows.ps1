#Requires -Version 5.1
<#
.SYNOPSIS
  Pack a portable Windows zip of SkeletonHive (Release exe + license docs).

.EXAMPLE
  .\scripts\pack-windows.ps1
  .\scripts\pack-windows.ps1 -Config Release -OutDir dist
#>
[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug')]
    [string] $Config = 'Release',

    [string] $BuildDir = 'build',

    [string] $OutDir = 'dist',

    [string] $Version = ''
)

$ErrorActionPreference = 'Stop'

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
Set-Location $RepoRoot

$ExePath = Join-Path $RepoRoot "$BuildDir\SkeletonHive_artefacts\$Config\SkeletonHive.exe"
if (-not (Test-Path -LiteralPath $ExePath)) {
    throw "Missing executable: $ExePath`nBuild Release first, e.g.:`n  cmake --build build --config Release"
}

if (-not $Version) {
    $CmakeLists = Get-Content -LiteralPath (Join-Path $RepoRoot 'CMakeLists.txt') -Raw
    if ($CmakeLists -match 'project\s*\(\s*SkeletonHive\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)') {
        $Version = $Matches[1]
    }
    else {
        $Version = '0.0.0'
    }
}

$Stamp = Get-Date -Format 'yyyyMMdd'
try {
    $GitSha = (git -C $RepoRoot rev-parse --short HEAD 2>$null).Trim()
}
catch {
    $GitSha = 'nogit'
}
if (-not $GitSha) { $GitSha = 'nogit' }

$ZipName = "SkeletonHive-$Version-windows-x64-$Stamp-$GitSha.zip"
$StageRoot = Join-Path $RepoRoot (Join-Path $OutDir "SkeletonHive-$Version-windows-x64")
$ZipPath = Join-Path $RepoRoot (Join-Path $OutDir $ZipName)

if (Test-Path -LiteralPath $StageRoot) {
    Remove-Item -LiteralPath $StageRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $StageRoot -Force | Out-Null

$FilesToCopy = @(
    @{ Src = $ExePath; Dst = 'SkeletonHive.exe' }
    @{ Src = (Join-Path $RepoRoot 'LICENSE'); Dst = 'LICENSE' }
    @{ Src = (Join-Path $RepoRoot 'NOTICE'); Dst = 'NOTICE' }
    @{ Src = (Join-Path $RepoRoot 'packaging\README-WINDOWS.txt'); Dst = 'README.txt' }
)

foreach ($item in $FilesToCopy) {
    if (-not (Test-Path -LiteralPath $item.Src)) {
        throw "Missing required file for package: $($item.Src)"
    }
    Copy-Item -LiteralPath $item.Src -Destination (Join-Path $StageRoot $item.Dst) -Force
}

New-Item -ItemType Directory -Path (Split-Path -Parent $ZipPath) -Force | Out-Null
if (Test-Path -LiteralPath $ZipPath) {
    Remove-Item -LiteralPath $ZipPath -Force
}

Compress-Archive -Path (Join-Path $StageRoot '*') -DestinationPath $ZipPath -CompressionLevel Optimal

$ExeSizeMb = [math]::Round((Get-Item -LiteralPath $ExePath).Length / 1MB, 1)
$ZipSizeMb = [math]::Round((Get-Item -LiteralPath $ZipPath).Length / 1MB, 1)

Write-Host "Packed: $ZipPath"
Write-Host "  exe:  $ExeSizeMb MB ($Config)"
Write-Host "  zip:  $ZipSizeMb MB"
Write-Host "  ver:  $Version ($GitSha)"

# Machine-readable path for CI
Write-Output $ZipPath
