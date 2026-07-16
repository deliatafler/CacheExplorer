param(
    [string]$BuildDir = "build-qt-prebuilt",
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Release",
    [string]$QtBinDir = "",
    [string]$VCRedistDir = "",
    [string]$OutputDir = "artifacts/cacheexplorer-qt-shared",
    [switch]$Zip,
    [string]$ZipPath = "",
    [switch]$NoChecksum
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

function Copy-PackageFile {
    param(
        [string]$Source,
        [string]$Destination
    )

    if (-not (Test-Path -LiteralPath $Source -PathType Leaf)) {
        throw "Package source file not found: $Source"
    }

    $destinationParent = Split-Path -Parent $Destination
    if ($destinationParent.Length -gt 0) {
        New-Item -ItemType Directory -Force -Path $destinationParent | Out-Null
    }

    Copy-Item -LiteralPath $Source -Destination $Destination -Force
}

function Get-CMakeCacheValue {
    param(
        [string]$BuildDirectory,
        [string]$Name,
        [string]$DefaultValue
    )

    $cacheFile = Join-Path $BuildDirectory "CMakeCache.txt"

    if (-not (Test-Path -LiteralPath $cacheFile -PathType Leaf)) {
        return $DefaultValue
    }

    $escapedName = [Regex]::Escape($Name)
    $line = Select-String `
        -LiteralPath $cacheFile `
        -Pattern "^$escapedName(:[^=]*)?=(.*)$" `
        -List

    if ($null -eq $line) {
        return $DefaultValue
    }

    return $line.Matches[0].Groups[2].Value
}

function Resolve-VCRedistDir {
    param([string]$RequestedVCRedistDir)

    if ($RequestedVCRedistDir.Length -gt 0) {
        $resolved = (Resolve-Path -LiteralPath $RequestedVCRedistDir).Path
        if (Test-Path -LiteralPath (Join-Path $resolved "vcruntime140.dll") -PathType Leaf) {
            return $resolved
        }

        throw "vcruntime140.dll was not found in -VCRedistDir $resolved."
    }

    $redistRoots = @()

    if ($env:VCINSTALLDIR -and $env:VCINSTALLDIR.Length -gt 0) {
        $redistRoots += Join-Path $env:VCINSTALLDIR "Redist\MSVC"
    }

    foreach ($programFiles in @($env:ProgramFiles, ${env:ProgramFiles(x86)})) {
        if ($programFiles -and $programFiles.Length -gt 0) {
            foreach ($edition in @("Community", "Professional", "Enterprise", "BuildTools")) {
                $redistRoots += Join-Path `
                    $programFiles `
                    "Microsoft Visual Studio\2022\$edition\VC\Redist\MSVC"
            }
        }
    }

    foreach ($redistRoot in $redistRoots) {
        if (-not (Test-Path -LiteralPath $redistRoot -PathType Container)) {
            continue
        }

        $versions = Get-ChildItem -LiteralPath $redistRoot -Directory |
            Sort-Object Name -Descending

        foreach ($version in $versions) {
            $x64Root = Join-Path $version.FullName "x64"

            if (-not (Test-Path -LiteralPath $x64Root -PathType Container)) {
                continue
            }

            $runtimeDirectories = Get-ChildItem -LiteralPath $x64Root -Directory |
                Where-Object { $_.Name -like "Microsoft.VC*.CRT" } |
                Sort-Object Name -Descending

            foreach ($runtimeDirectory in $runtimeDirectories) {
                if (Test-Path `
                        -LiteralPath (Join-Path $runtimeDirectory.FullName "vcruntime140.dll") `
                        -PathType Leaf) {
                    return $runtimeDirectory.FullName
                }
            }
        }
    }

    throw "The x64 Visual C++ redistributable DLLs were not found. Install the Visual Studio C++ build tools or pass -VCRedistDir C:\Path\To\Microsoft.VC*.CRT."
}

$resolvedBuildDir = Resolve-RepoPath $BuildDir
$resolvedOutputDir = Resolve-RepoPath $OutputDir
$packageVersion = Get-CMakeCacheValue `
    -BuildDirectory $resolvedBuildDir `
    -Name "CACHEEXPLORER_DISPLAY_VERSION" `
    -DefaultValue "unknown"
$builtExe = Join-Path $resolvedBuildDir "cachegui/$Configuration/CacheExplorer.exe"

if (-not (Test-Path -LiteralPath $builtExe -PathType Leaf)) {
    $legacyBuiltExe = Join-Path $resolvedBuildDir "cachegui/$Configuration/cachegui.exe"
    if (Test-Path -LiteralPath $legacyBuiltExe -PathType Leaf) {
        $builtExe = $legacyBuiltExe
    }
}

if (-not (Test-Path -LiteralPath $builtExe -PathType Leaf)) {
    throw "Built executable not found. Build cachegui first."
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

$resolvedVCRedistDir = Resolve-VCRedistDir $VCRedistDir
Get-ChildItem -LiteralPath $resolvedVCRedistDir -Filter "*.dll" -File |
    ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $resolvedOutputDir -Force
    }

Write-Host "Packaged Qt GUI:"
Write-Host "  $resolvedOutputDir"
Write-Host "Included Visual C++ runtime:"
Write-Host "  $resolvedVCRedistDir"

$packageDocsDir = Join-Path $resolvedOutputDir "docs"

Copy-PackageFile `
    -Source (Join-Path $repoRoot "README.md") `
    -Destination (Join-Path $resolvedOutputDir "README.md")

Copy-PackageFile `
    -Source (Join-Path $repoRoot "RELEASE_NOTES.md") `
    -Destination (Join-Path $resolvedOutputDir "RELEASE_NOTES.md")

Copy-PackageFile `
    -Source (Join-Path $repoRoot "docs/qt-user-guide.md") `
    -Destination (Join-Path $packageDocsDir "qt-user-guide.md")

$packageInfo = @(
    "CacheExplorer package"
    "Version: $packageVersion"
    "Configuration: $Configuration"
    "Build directory: $resolvedBuildDir"
    "Qt bin directory: $(Split-Path -Parent $windeployqt)"
    "Visual C++ runtime directory: $resolvedVCRedistDir"
    "Packaged at: $((Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ"))"
    ""
    "Run CacheExplorer.exe to open the Qt GUI."
    "See README.md, RELEASE_NOTES.md, and docs/qt-user-guide.md for beta notes."
)

Set-Content `
    -LiteralPath (Join-Path $resolvedOutputDir "PACKAGE_INFO.txt") `
    -Value $packageInfo `
    -Encoding ASCII

Write-Host "Included package notes:"
Write-Host "  README.md"
Write-Host "  RELEASE_NOTES.md"
Write-Host "  docs/qt-user-guide.md"
Write-Host "  PACKAGE_INFO.txt"

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

    if (-not $NoChecksum) {
        $hash = Get-FileHash -Algorithm SHA256 -LiteralPath $resolvedZipPath
        $checksumPath = "$resolvedZipPath.sha256"
        $checksumLine = "$($hash.Hash.ToLowerInvariant())  $(Split-Path -Leaf $resolvedZipPath)"

        Set-Content `
            -LiteralPath $checksumPath `
            -Value $checksumLine `
            -Encoding ASCII

        Write-Host "Created SHA256 checksum:"
        Write-Host "  $checksumPath"
        Write-Host "  $checksumLine"
    }
}
