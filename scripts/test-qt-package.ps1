param(
    [string]$PackageDir = "artifacts/cacheexplorer-qt-shared",
    [string]$ZipPath = "",
    [string]$ChecksumPath = "",
    [switch]$Launch,
    [switch]$ExtractAndLaunch,
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

function Assert-FileExists {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required package file not found: $Path"
    }
}

function Test-ZipEntry {
    param(
        [System.IO.Compression.ZipArchive]$Archive,
        [string]$EntryName
    )

    $normalizedName = $EntryName.Replace("\", "/")

    foreach ($entry in $Archive.Entries) {
        if ($entry.FullName.Replace("\", "/") -eq $normalizedName) {
            return $true
        }
    }

    return $false
}

$resolvedPackageDir = Resolve-RepoPath $PackageDir

if (-not (Test-Path -LiteralPath $resolvedPackageDir -PathType Container)) {
    throw "Package directory not found: $resolvedPackageDir"
}

$requiredFiles = @(
    "CacheExplorer.exe",
    "Qt6Core.dll",
    "Qt6Gui.dll",
    "Qt6Widgets.dll",
    "PACKAGE_INFO.txt",
    "README.md",
    "RELEASE_NOTES.md",
    "docs/qt-user-guide.md",
    "platforms/qwindows.dll"
)

foreach ($file in $requiredFiles) {
    Assert-FileExists (Join-Path $resolvedPackageDir $file)
}

Write-Host "Package directory contains required files:"
Write-Host "  $resolvedPackageDir"

if ($ZipPath.Length -gt 0) {
    $resolvedZipPath = Resolve-RepoPath $ZipPath
    Assert-FileExists $resolvedZipPath

    Add-Type -AssemblyName System.IO.Compression.FileSystem

    $archive = [System.IO.Compression.ZipFile]::OpenRead($resolvedZipPath)

    try {
        foreach ($file in $requiredFiles) {
            if (-not (Test-ZipEntry -Archive $archive -EntryName $file)) {
                throw "Required package archive entry not found: $file"
            }
        }
    }
    finally {
        $archive.Dispose()
    }

    Write-Host "Package archive contains required files:"
    Write-Host "  $resolvedZipPath"

    if ($ChecksumPath.Length -gt 0) {
        $resolvedChecksumPath = Resolve-RepoPath $ChecksumPath
    } else {
        $resolvedChecksumPath = "$resolvedZipPath.sha256"
    }

    Assert-FileExists $resolvedChecksumPath

    $checksumLine =
        (Get-Content -LiteralPath $resolvedChecksumPath -TotalCount 1).Trim()
    $expectedHash = ($checksumLine -split "\s+")[0].ToLowerInvariant()
    $actualHash =
        (Get-FileHash -Algorithm SHA256 -LiteralPath $resolvedZipPath).
            Hash.ToLowerInvariant()

    if ($expectedHash -ne $actualHash) {
        throw "Package checksum mismatch. Expected $expectedHash, got $actualHash."
    }

    Write-Host "Package checksum matches:"
    Write-Host "  $expectedHash"
}

$launchPackageDir = $resolvedPackageDir
$extractionDirectory = ""

if ($ExtractAndLaunch) {
    if ($ZipPath.Length -eq 0) {
        throw "-ExtractAndLaunch requires -ZipPath."
    }

    $extractionDirectory = Join-Path `
        ([System.IO.Path]::GetTempPath()) `
        ("CacheExplorer-package-smoke-" + [System.Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path $extractionDirectory | Out-Null
    try {
        Expand-Archive -LiteralPath $resolvedZipPath -DestinationPath $extractionDirectory

        foreach ($file in $requiredFiles) {
            Assert-FileExists (Join-Path $extractionDirectory $file)
        }
    }
    catch {
        Remove-Item -LiteralPath $extractionDirectory -Recurse -Force
        throw
    }

    $launchPackageDir = $extractionDirectory
    Write-Host "Package archive extracted for clean launch smoke:"
    Write-Host "  $extractionDirectory"
}

if ($Launch -or $ExtractAndLaunch) {
    if ($LaunchSeconds -lt 1) {
        throw "LaunchSeconds must be at least 1."
    }

    $exePath = Join-Path $launchPackageDir "CacheExplorer.exe"

    $process = $null

    try {
        $process = Start-Process `
            -FilePath $exePath `
            -WorkingDirectory $launchPackageDir `
            -WindowStyle Hidden `
            -PassThru

        Start-Sleep -Seconds $LaunchSeconds

        if ($process.HasExited) {
            throw "Packaged CacheExplorer exited with code $($process.ExitCode)."
        }

        $process.Refresh()
        if ($process.MainWindowTitle -like "*System Error*") {
            throw "Packaged CacheExplorer opened a system error dialog: $($process.MainWindowTitle)"
        }

        Write-Host "Package launch smoke passed:"
        Write-Host "  Process $($process.Id) stayed running for $LaunchSeconds second(s)."
    }
    finally {
        if ($null -ne $process -and -not $process.HasExited) {
            Stop-Process -Id $process.Id -Force
        }

        if ($extractionDirectory.Length -gt 0 -and
            (Test-Path -LiteralPath $extractionDirectory -PathType Container)) {
            Remove-Item -LiteralPath $extractionDirectory -Recurse -Force
        }
    }
}
