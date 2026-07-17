param(
    [Parameter(Mandatory = $true)]
    [string]$InstallerPath,
    [int]$LaunchSeconds = 5
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ($LaunchSeconds -lt 1) {
    throw "LaunchSeconds must be at least 1."
}

$resolvedInstaller = (Resolve-Path -LiteralPath $InstallerPath).Path
$uninstallRegistryPaths = @(
    "HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\CacheExplorer",
    "HKLM:\Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\CacheExplorer"
)
function Test-RegisteredInstallation {
    foreach ($registryPath in $uninstallRegistryPaths) {
        if (Test-Path -LiteralPath $registryPath) {
            return $true
        }
    }
    return $false
}

if (Test-RegisteredInstallation) {
    throw "Refusing to replace an existing CacheExplorer installation during the smoke test."
}

$resolvedTempRoot = [System.IO.Path]::GetFullPath($env:TEMP)
$resolvedInstallDir = [System.IO.Path]::GetFullPath(
    (Join-Path $resolvedTempRoot "CacheExplorer-installer-smoke-$PID"))

if (-not $resolvedInstallDir.StartsWith(
        "$resolvedTempRoot$([System.IO.Path]::DirectorySeparatorChar)",
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Installer smoke directory must remain under the system temp directory."
}

if (Test-Path -LiteralPath $resolvedInstallDir) {
    throw "Installer smoke directory already exists: $resolvedInstallDir"
}

function Invoke-CheckedProcess {
    param(
        [string]$FilePath,
        [string[]]$ArgumentList
    )

    $process = Start-Process `
        -FilePath $FilePath `
        -ArgumentList $ArgumentList `
        -PassThru `
        -Wait `
        -WindowStyle Hidden
    if ($process.ExitCode -ne 0) {
        throw "$FilePath exited with code $($process.ExitCode)."
    }
}

$uninstaller = Join-Path $resolvedInstallDir "Uninstall.exe"
$uninstallCompleted = $false

try {
    Invoke-CheckedProcess `
        -FilePath $resolvedInstaller `
        -ArgumentList @("/S", "/D=$resolvedInstallDir")

    $installedExe = Join-Path $resolvedInstallDir "bin\CacheExplorer.exe"
    $requiredFiles = @(
        $installedExe,
        $uninstaller,
        (Join-Path $resolvedInstallDir "bin\Qt6Core.dll"),
        (Join-Path $resolvedInstallDir "bin\Qt6Widgets.dll"),
        (Join-Path $resolvedInstallDir "bin\vcruntime140.dll"),
        (Join-Path $resolvedInstallDir "plugins\platforms\qwindows.dll")
    )
    foreach ($requiredFile in $requiredFiles) {
        if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
            throw "Installed file not found: $requiredFile"
        }
    }

    $launch = Start-Process `
        -FilePath $installedExe `
        -WorkingDirectory (Split-Path -Parent $installedExe) `
        -PassThru `
        -WindowStyle Hidden
    try {
        Start-Sleep -Seconds $LaunchSeconds
        $launch.Refresh()
        if ($launch.HasExited) {
            throw "Installed CacheExplorer exited with code $($launch.ExitCode)."
        }
    }
    finally {
        if (-not $launch.HasExited) {
            Stop-Process -Id $launch.Id -Force
        }
    }

    Invoke-CheckedProcess `
        -FilePath $uninstaller `
        -ArgumentList @("/S")
    $uninstallCompleted = $true

    for ($attempt = 0; $attempt -lt 20; ++$attempt) {
        if (-not (Test-Path -LiteralPath $installedExe -PathType Leaf) -and
            -not (Test-RegisteredInstallation)) {
            break
        }
        Start-Sleep -Milliseconds 250
    }

    if (Test-Path -LiteralPath $installedExe -PathType Leaf) {
        throw "Uninstall left the application executable in place."
    }
    if (Test-RegisteredInstallation) {
        throw "Uninstall left the application registered."
    }

    Write-Host "Windows installer smoke passed:"
    Write-Host "  Installer: $resolvedInstaller"
    Write-Host "  Install directory: $resolvedInstallDir"
}
finally {
    if (-not $uninstallCompleted -and
        (Test-Path -LiteralPath $uninstaller -PathType Leaf)) {
        try {
            Invoke-CheckedProcess `
                -FilePath $uninstaller `
                -ArgumentList @("/S")
        }
        catch {
            Write-Warning "Cleanup uninstall failed: $($_.Exception.Message)"
        }
    }

    if (Test-Path -LiteralPath $resolvedInstallDir) {
        Remove-Item -LiteralPath $resolvedInstallDir -Recurse -Force
    }
}
