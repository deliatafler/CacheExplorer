param(
    [string]$BuildDir = "build-qt-prebuilt",
    [string]$QtDir = "",
    [string]$VcpkgRoot = $env:VCPKG_ROOT,
    [string]$Triplet = "x64-windows-static-md",
    [switch]$KeepCache,
    [switch]$Build
)

$ErrorActionPreference = "Stop"

function Resolve-QtDir {
    param([string]$RequestedQtDir)

    if ($RequestedQtDir.Length -gt 0) {
        return (Resolve-Path -LiteralPath $RequestedQtDir).Path
    }

    if ($env:QT_DIR -and $env:QT_DIR.Length -gt 0) {
        return (Resolve-Path -LiteralPath $env:QT_DIR).Path
    }

    $candidates = @()

    if (Test-Path -LiteralPath "C:\Qt" -PathType Container) {
        $candidates += Get-ChildItem -LiteralPath "C:\Qt" -Directory |
            Sort-Object Name -Descending |
            ForEach-Object {
                Join-Path $_.FullName "msvc2022_64"
            }
    }

    if (Test-Path -LiteralPath ".qt-prebuilt-test\Qt" -PathType Container) {
        $candidates += Get-ChildItem -LiteralPath ".qt-prebuilt-test\Qt" -Directory |
            Sort-Object Name -Descending |
            ForEach-Object {
                Join-Path $_.FullName "msvc2022_64"
            }
    }

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath (Join-Path $candidate "lib\cmake\Qt6\Qt6Config.cmake") -PathType Leaf) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    throw "Qt was not found. Install the official Qt 6 MSVC 2022 64-bit SDK, then pass -QtDir C:\Qt\<version>\msvc2022_64 or set QT_DIR."
}

function Resolve-VcpkgRoot {
    param([string]$RequestedVcpkgRoot)

    if ($RequestedVcpkgRoot -and $RequestedVcpkgRoot.Length -gt 0) {
        return (Resolve-Path -LiteralPath $RequestedVcpkgRoot).Path
    }

    $candidates = @(
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\vcpkg",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\vcpkg",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\vcpkg"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath (Join-Path $candidate "scripts\buildsystems\vcpkg.cmake") -PathType Leaf) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    throw "vcpkg was not found. Set VCPKG_ROOT, pass -VcpkgRoot C:\Path\To\vcpkg, or install the Visual Studio bundled vcpkg component."
}

$resolvedVcpkgRoot = Resolve-VcpkgRoot $VcpkgRoot
$toolchain = Join-Path $resolvedVcpkgRoot "scripts\buildsystems\vcpkg.cmake"
if (-not (Test-Path -LiteralPath $toolchain -PathType Leaf)) {
    throw "vcpkg CMake toolchain not found at: $toolchain"
}

$resolvedQtDir = Resolve-QtDir $QtDir

Write-Host "Configuring CacheExplorer Qt GUI"
Write-Host "  Build directory: $BuildDir"
Write-Host "  Qt SDK:          $resolvedQtDir"
Write-Host "  vcpkg triplet:   $Triplet"

$freshArgs = @()
if (-not $KeepCache) {
    $freshArgs += "--fresh"
}

cmake -S . -B $BuildDir @freshArgs -A x64 `
    "-DCMAKE_TOOLCHAIN_FILE=$toolchain" `
    "-DVCPKG_TARGET_TRIPLET=$Triplet" `
    "-DCMAKE_PREFIX_PATH=$resolvedQtDir" `
    "-DCACHEEXPLORER_BUILD_QT_GUI=ON" `
    "-DCACHEEXPLORER_STATIC_MSVC_RUNTIME=OFF"

if ($LASTEXITCODE -ne 0) {
    throw "CMake configure failed with exit code $LASTEXITCODE"
}

if ($Build) {
    cmake --build $BuildDir --config Release --target cachegui_qt
    if ($LASTEXITCODE -ne 0) {
        throw "Qt GUI build failed with exit code $LASTEXITCODE"
    }
}
