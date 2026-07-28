# flashauto.ps1 - build-adjacent flash (+reset) helper, board selected by PKOB4 serial.
#
# Output: by default the flash/reset tools now print the compact [flash]/[reset]
# progress log with a 5s heartbeat (proof-of-life during long waits) and pass the
# MPLAB X report block through. Pass -Verbose to instead stream the raw mdb /
# IPECMDBoost output (the previous behaviour), or -Quiet to suppress this wrapper's
# own status lines. See resetauto.ps1 for a reset-only shortcut.

param(
    [switch]$Reset,
    [switch]$List,
    [switch]$DryRun,
    [switch]$Verbose,
    [switch]$Quiet,
    [Alias('clean-java')]
    [switch]$CleanJava,
    [string]$Serial = $env:PKOB4_SERIAL,
    [string]$Device = 'dsPIC33AK512MPS512',
    [string]$ResetDevice,
    [string]$Hex,
    [string]$Configuration,
    [string]$Root = (Get-Location).Path,
    [string]$ProjectDir,
    [string]$ToolsDir = $env:FLASH_RESET_TOOLS,
    [int]$ResetTimeoutSec = 120
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'hal_starter_build_state.ps1')

if ($Verbose -and $Quiet) {
    throw "Use either -Verbose or -Quiet, not both."
}

function Write-Status {
    param(
        [string]$Message
    )

    if (-not $Quiet) {
        Write-Host $Message
    }
}

function Resolve-FlashResetToolsDir {
    param(
        [string]$RequestedToolsDir,
        [string]$Root
    )

    if (-not [string]::IsNullOrWhiteSpace($RequestedToolsDir)) {
        return (Resolve-Path -LiteralPath $RequestedToolsDir).Path
    }

    $scriptToolsDir = Join-Path $PSScriptRoot '_flash_reset_tools'
    if (Test-Path -LiteralPath $scriptToolsDir) {
        return (Resolve-Path -LiteralPath $scriptToolsDir).Path
    }

    $repoToolsDir = Join-Path $Root '_flash_reset_tools'
    if (Test-Path -LiteralPath $repoToolsDir) {
        return (Resolve-Path -LiteralPath $repoToolsDir).Path
    }

    $siblingToolsDir = Join-Path (Split-Path -Parent $Root) '_flash_reset_tools'
    if (Test-Path -LiteralPath $siblingToolsDir) {
        return (Resolve-Path -LiteralPath $siblingToolsDir).Path
    }

    throw "Flash/reset tools directory not found. Expected .\buildtools\_flash_reset_tools, or set FLASH_RESET_TOOLS / pass -ToolsDir."
}

function Get-ConnectedPkob4Serials {
    param(
        [string]$FlashTool
    )

    Write-Status "Checking connected PKOB4 serials..."
    $output = & $FlashTool --list 2>&1
    if ($LASTEXITCODE -ne 0) {
        $output | Write-Host
        throw "PKOB4 list failed with exit code $LASTEXITCODE"
    }

    return @($output | ForEach-Object {
        if ($_ -match '^\s*([0-9A-Z]{10,})\s*$') {
            $matches[1]
        }
    })
}

function Resolve-Pkob4Serial {
    param(
        [string]$RequestedSerial,
        [string]$FlashTool
    )

    if (-not [string]::IsNullOrWhiteSpace($RequestedSerial)) {
        return $RequestedSerial
    }

    $serials = @(Get-ConnectedPkob4Serials -FlashTool $FlashTool)
    if ($serials.Count -eq 1) {
        Write-Status "Serial: $($serials[0]) (auto-detected)"
        return $serials[0]
    }
    if ($serials.Count -eq 0) {
        Write-Host "No connected PKOB4 serial found."
        Write-Host "Connect one target, or pass -Serial if the tool list is not available."
        exit 2
    }

    Write-Host "Multiple PKOB4 serials found. Refusing to choose a target automatically."
    Write-Host "Connected serials:"
    foreach ($serial in $serials) {
        Write-Host "  $serial"
    }
    Write-Host ""
    Write-Host "Run again with an explicit serial, for example:"
    Write-Host "  .\buildtools\flashauto.ps1 -Serial $($serials[0])"
    Write-Host "  .\buildtools\flashauto.ps1 -Reset -Serial $($serials[0])"
    exit 2
}

function Resolve-ProductionHex {
    param(
        [string]$RequestedHex,
        [string]$ProjectDir,
        [string]$Configuration
    )

    if (-not [string]::IsNullOrWhiteSpace($RequestedHex)) {
        return (Resolve-Path -LiteralPath $RequestedHex).Path
    }

    $projectName = Split-Path -Leaf $ProjectDir
    $prodDir = Join-Path $ProjectDir "dist\$Configuration\production"
    $hexPath = Join-Path $prodDir "$projectName.production.bundle.hex"
    $verifyReport = Join-Path $prodDir "$projectName.production.bundle.verify_report.txt"

    if (-not (Test-Path -LiteralPath $hexPath)) {
        Write-Status "Expected verified bundle: $hexPath"
        throw "Dual-partition bundle not found. Run buildtools\build.ps1 first."
    }
    if (-not (Test-Path -LiteralPath $verifyReport)) {
        throw "Bundle verification report not found: $verifyReport"
    }
    $reportText = [System.IO.File]::ReadAllText($verifyReport)
    if ($reportText -notmatch '(?m)^\s*PASS') {
        throw "Bundle verification is not PASS: $verifyReport. Refusing to flash."
    }
    $hashMatch = [regex]::Match($reportText, '(?im)^\s*\[INFO\]\s+bundle_sha256=([0-9a-f]{64})\s*$')
    if (-not $hashMatch.Success) {
        throw "Bundle verification report has no SHA-256 attestation: $verifyReport. Rebuild before flashing."
    }
    $actualHash = (Get-FileHash -LiteralPath $hexPath -Algorithm SHA256).Hash
    if (-not $actualHash.Equals($hashMatch.Groups[1].Value,
                                [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Bundle SHA-256 does not match its PASS report. Rebuild before flashing."
    }

    return (Resolve-Path -LiteralPath $hexPath).Path
}

function Convert-ToResetDevice {
    param(
        [string]$Device
    )

    if ($Device.StartsWith('dsPIC', [System.StringComparison]::OrdinalIgnoreCase)) {
        return $Device.Substring(5)
    }

    return $Device
}

function Invoke-CheckedExe {
    param(
        [string]$Exe,
        [string[]]$Arguments
    )

    Write-Status "==> $Exe $($Arguments -join ' ')"
    & $Exe @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$(Split-Path -Leaf $Exe) failed with exit code $LASTEXITCODE"
    }
}

function Invoke-ResetJavaCleanup {
    param(
        [string]$ResetTool,
        [bool]$DryRun
    )

    if ($DryRun) {
        Write-Status "Dry-run: would clean PKOB4 Boost Java state before reset."
        return
    }

    $cleanupArgs = @('--clean-java')
    if (-not $Quiet) { $cleanupArgs += '--verbose' }

    Write-Status "Cleaning PKOB4 Boost Java state before reset..."
    Invoke-CheckedExe -Exe $ResetTool -Arguments $cleanupArgs
}

$repoRoot = Resolve-HalStarterRepoRoot -RequestedRoot $Root
$projectDir = Resolve-HalStarterProjectDir -RepoRoot $repoRoot -RequestedProjectDir $ProjectDir
$configurations = Get-HalStarterConfigurations -ProjectDir $projectDir
if ([string]::IsNullOrWhiteSpace($Configuration)) {
    $Configuration = Get-HalStarterActiveConfiguration -ProjectDir $projectDir -Configurations $configurations
}
$toolsDir = Resolve-FlashResetToolsDir -RequestedToolsDir $ToolsDir -Root $repoRoot
$flashTool = Join-Path $toolsDir 'flash_pkob4.exe'
$resetTool = Join-Path $toolsDir 'reset_pkob4.exe'

if (-not (Test-Path -LiteralPath $flashTool)) {
    throw "flash_pkob4.exe not found: $flashTool"
}
if (-not (Test-Path -LiteralPath $resetTool)) {
    throw "reset_pkob4.exe not found: $resetTool"
}

if ($List) {
    Write-Status "flashauto: list connected PKOB4 targets"
    Invoke-CheckedExe -Exe $flashTool -Arguments @('--list')
    return
}

if ($Reset) {
    Write-Status "flashauto: reset only"
} else {
    Write-Status "flashauto: flash + reset"
}

$serialNumber = Resolve-Pkob4Serial -RequestedSerial $Serial -FlashTool $flashTool
$resetDeviceToken = if ([string]::IsNullOrWhiteSpace($ResetDevice)) {
    Convert-ToResetDevice -Device $Device
} else {
    $ResetDevice
}

Write-Status "Root: $repoRoot"
Write-Status "Project: $projectDir"
Write-Status "Tools: $toolsDir"
Write-Status "Serial: $serialNumber"

if ($Reset) {
    $resetArgs = @('--serial', $serialNumber, '--device', $resetDeviceToken, '--timeout', $ResetTimeoutSec)
    if ($Verbose) { $resetArgs += '--verbose' }
    if ($DryRun) { $resetArgs += '--dry-run' }
    Write-Status "Reset device token: $resetDeviceToken"
    Write-Status "Reset timeout: ${ResetTimeoutSec}s"
    if ($CleanJava) {
        Invoke-ResetJavaCleanup -ResetTool $resetTool -DryRun $DryRun
    }
    Invoke-CheckedExe -Exe $resetTool -Arguments $resetArgs
    Write-Status "flashauto: reset completed"
    return
}

$hexPath = Resolve-ProductionHex -RequestedHex $Hex -ProjectDir $projectDir -Configuration $Configuration
Write-Status "Configuration: $Configuration"
# APP_BUILD is not part of the HEX path, so say which variation the last build of
# this configuration produced (stamped by build.ps1; unknown after an IDE build).
$builtAppBuild = Get-HalStarterBuiltPreset -ProjectDir $projectDir -Configuration $Configuration
if ($builtAppBuild) {
    Write-Status "Last build of this configuration: $builtAppBuild"
} else {
    Write-Status "Last build of this configuration: unknown (not built by build.ps1)"
}
Write-Status "Flash device token: $Device"
Write-Status "Reset device token: $resetDeviceToken"
Write-Status "HEX: $hexPath"

$flashArgs = @(
    '--serial', $serialNumber,
    '--device', $Device,
    '--hex', $hexPath
)
if ($Verbose) { $flashArgs += '--verbose' }
if ($DryRun) { $flashArgs += '--dry-run' }

Invoke-CheckedExe -Exe $flashTool -Arguments $flashArgs

$resetArgs = @('--serial', $serialNumber, '--device', $resetDeviceToken, '--timeout', $ResetTimeoutSec)
if ($Verbose) { $resetArgs += '--verbose' }
if ($DryRun) { $resetArgs += '--dry-run' }

Write-Status "Running reset after successful flash..."
Write-Status "Reset timeout: ${ResetTimeoutSec}s"
if ($CleanJava) {
    Invoke-ResetJavaCleanup -ResetTool $resetTool -DryRun $DryRun
}
$resetStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
Invoke-CheckedExe -Exe $resetTool -Arguments $resetArgs
$resetStopwatch.Stop()
Write-Status ("Reset elapsed: {0:N1}s" -f $resetStopwatch.Elapsed.TotalSeconds)
Write-Status "flashauto: flash + reset completed"
