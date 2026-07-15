param(
    [string]$BuildDir = "build-qt-prebuilt",
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Release",
    [string]$QtBinDir = "",
    [string]$OutputDir = "artifacts/cacheexplorer-qt-shared",
    [switch]$Zip,
    [string]$ZipPath = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptRoot

function Resolve-RepoPath {
    param([string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $repoRoot $Path))
}

$resolvedBuildDir = Resolve-RepoPath $BuildDir
$resolvedOutputDir = Resolve-RepoPath $OutputDir
$builtExe = Join-Path $resolvedBuildDir "cachegui_qt/$Configuration/CacheExplorer.exe"

if (-not (Test-Path -LiteralPath $builtExe -PathType Leaf)) {
    $legacyBuiltExe = Join-Path $resolvedBuildDir "cachegui_qt/$Configuration/cachegui_qt.exe"
    if (Test-Path -LiteralPath $legacyBuiltExe -PathType Leaf) {
        $builtExe = $legacyBuiltExe
    }
}

if (-not (Test-Path -LiteralPath $builtExe -PathType Leaf)) {
    throw "Built executable not found. Build cachegui_qt first."
}

if ([string]::IsNullOrWhiteSpace($env:VCINSTALLDIR)) {
    Write-Warning "VCINSTALLDIR is not set. For packages you plan to share, run this from a Visual Studio developer shell."
}

if ($QtBinDir.Length -gt 0) {
    $windeployqt = Join-Path (Resolve-RepoPath $QtBinDir) "windeployqt.exe"
    if (-not (Test-Path -LiteralPath $windeployqt -PathType Leaf)) {
        throw "windeployqt.exe not found at: $windeployqt"
    }
} else {
    $command = Get-Command "windeployqt.exe" -ErrorAction SilentlyContinue
    if ($null -eq $command) {
        throw "windeployqt.exe was not found on PATH. Pass -QtBinDir C:\Path\To\Qt\bin."
    }
    $windeployqt = $command.Source
}

New-Item -ItemType Directory -Force -Path $resolvedOutputDir | Out-Null

$packageExe = Join-Path $resolvedOutputDir "CacheExplorer.exe"
Copy-Item -LiteralPath $builtExe -Destination $packageExe -Force

& $windeployqt --release --dir $resolvedOutputDir $packageExe
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed with exit code $LASTEXITCODE"
}

Write-Host "Packaged Qt GUI:"
Write-Host "  $resolvedOutputDir"

if ($Zip) {
    if ($ZipPath.Length -gt 0) {
        $resolvedZipPath = Resolve-RepoPath $ZipPath
    } else {
        $resolvedZipPath = "$resolvedOutputDir.zip"
    }

    $zipParent = Split-Path -Parent $resolvedZipPath
    if ($zipParent.Length -gt 0) {
        New-Item -ItemType Directory -Force -Path $zipParent | Out-Null
    }

    Compress-Archive `
        -Path (Join-Path $resolvedOutputDir "*") `
        -DestinationPath $resolvedZipPath `
        -Force

    Write-Host "Created package archive:"
    Write-Host "  $resolvedZipPath"
}
