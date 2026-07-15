param(
    [string]$BuildDir = "build-qt-prebuilt",
    [string]$QtDir = "",
    [string]$Configuration = "Release",
    [switch]$SmokeTest,
    [int]$LaunchSeconds = 5
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

function Resolve-QtDir {
    param([string]$RequestedQtDir)

    if ($RequestedQtDir.Length -gt 0) {
        return (Resolve-Path -LiteralPath $RequestedQtDir).Path
    }

    if ($env:QT_DIR -and $env:QT_DIR.Length -gt 0) {
        return (Resolve-Path -LiteralPath $env:QT_DIR).Path
    }

    if (Test-Path -LiteralPath "C:\Qt" -PathType Container) {
        $candidates =
            Get-ChildItem -LiteralPath "C:\Qt" -Directory |
            Sort-Object Name -Descending |
            ForEach-Object {
                Join-Path $_.FullName "msvc2022_64"
            }

        foreach ($candidate in $candidates) {
            if (Test-Path -LiteralPath (Join-Path $candidate "bin\Qt6Widgets.dll") -PathType Leaf) {
                return (Resolve-Path -LiteralPath $candidate).Path
            }
        }
    }

    throw "Qt was not found. Pass -QtDir C:\Qt\<version>\msvc2022_64 or set QT_DIR."
}

if ($LaunchSeconds -lt 1) {
    throw "LaunchSeconds must be at least 1."
}

$resolvedBuildDir = Resolve-RepoPath $BuildDir
$exePath = Join-Path $resolvedBuildDir "cachegui_qt\$Configuration\CacheExplorer.exe"
if (-not (Test-Path -LiteralPath $exePath -PathType Leaf)) {
    throw "CacheExplorer.exe not found at: $exePath"
}

$resolvedQtDir = Resolve-QtDir $QtDir
$qtBinDir = Join-Path $resolvedQtDir "bin"
if (-not (Test-Path -LiteralPath (Join-Path $qtBinDir "Qt6Widgets.dll") -PathType Leaf)) {
    throw "Qt6Widgets.dll not found in Qt bin directory: $qtBinDir"
}

$oldPath = $env:Path
try {
    $env:Path = "$qtBinDir;$oldPath"

    $startInfo = @{
        FilePath = $exePath
        WorkingDirectory = Split-Path -Parent $exePath
        PassThru = $true
    }

    if ($SmokeTest) {
        $startInfo.WindowStyle = "Hidden"
    }

    $process = Start-Process @startInfo

    if (-not $SmokeTest) {
        Write-Host "Launched CacheExplorer:"
        Write-Host "  $exePath"
        Write-Host "Using Qt runtime:"
        Write-Host "  $qtBinDir"
        return
    }

    try {
        Start-Sleep -Seconds $LaunchSeconds
        $process.Refresh()

        if ($process.HasExited) {
            throw "CacheExplorer exited with code $($process.ExitCode)."
        }

        if ($process.MainWindowTitle -like "*System Error*") {
            throw "CacheExplorer opened a system error dialog: $($process.MainWindowTitle)"
        }

        Write-Host "Qt prebuilt launch smoke passed:"
        Write-Host "  Process $($process.Id) stayed running for $LaunchSeconds second(s)."
        Write-Host "  Qt runtime: $qtBinDir"
    }
    finally {
        if (-not $process.HasExited) {
            Stop-Process -Id $process.Id -Force
        }
    }
}
finally {
    $env:Path = $oldPath
}
