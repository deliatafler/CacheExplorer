param(
    [string]$BuildDir = "build-qt-prebuilt",
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Release",
    [string]$QtBinDir = "",
    [string]$OutputDir = "artifacts/cacheexplorer-qt-shared"
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
$builtExe = Join-Path $resolvedBuildDir "cachegui_qt/$Configuration/cachegui_qt.exe"

if (-not (Test-Path -LiteralPath $builtExe -PathType Leaf)) {
    throw "Built executable not found: $builtExe. Build cachegui_qt first."
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

$packageExe = Join-Path $resolvedOutputDir "cachegui_qt.exe"
Copy-Item -LiteralPath $builtExe -Destination $packageExe -Force

& $windeployqt --release --dir $resolvedOutputDir $packageExe
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed with exit code $LASTEXITCODE"
}

Write-Host "Packaged Qt GUI:"
Write-Host "  $resolvedOutputDir"
