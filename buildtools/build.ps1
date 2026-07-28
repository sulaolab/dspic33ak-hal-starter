param(
    [switch]$Full,
    [switch]$Clean,
    [switch]$Generate,
    [string]$Configuration,
    [string]$Preset,
    [string]$Root = (Get-Location).Path,
    [string]$ProjectDir,
    [int]$Jobs = [Math]::Min([Environment]::ProcessorCount, 8)
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'hal_starter_build_state.ps1')

function Invoke-CheckedCommand {
    param(
        [Parameter(Mandatory = $true)]
        [scriptblock]$Command,
        [Parameter(Mandatory = $true)]
        [string]$Description
    )

    Write-Host "==> $Description"
    & $Command
    if ($LASTEXITCODE -ne 0) {
        throw "$Description failed with exit code $LASTEXITCODE"
    }
}

function Get-MplabXInstallRoots {
    $roots = @()
    foreach ($base in @($env:ProgramFiles, ${env:ProgramFiles(x86)})) {
        if (-not [string]::IsNullOrWhiteSpace($base)) {
            $roots += (Join-Path $base 'Microchip\MPLABX')
        }
    }
    $roots += @(
        'C:\Program Files\Microchip\MPLABX',
        'C:\Program Files (x86)\Microchip\MPLABX'
    )
    $roots = $roots | Select-Object -Unique

    foreach ($root in $roots) {
        if (-not (Test-Path -LiteralPath $root)) {
            continue
        }

        Get-ChildItem -LiteralPath $root -Directory -Filter 'v*' -ErrorAction SilentlyContinue |
            ForEach-Object {
                $versionText = $_.Name.TrimStart('v')
                $version = $null
                if (-not [System.Version]::TryParse($versionText, [ref]$version)) {
                    return
                }

                [pscustomobject]@{
                    Path = $_.FullName
                    Version = $version
                    VersionLabel = $_.Name
                }
            }
    }
}

function Get-MplabXVersionLabelFromPath {
    param(
        [string]$Path
    )

    if ($Path -match '[\\/]MPLABX[\\/](v[^\\/]+)[\\/]') {
        return $matches[1]
    }

    return 'custom'
}

function Resolve-MplabXTool {
    param(
        [string]$ToolName,
        [string]$RelativePath,
        [string]$OverridePath,
        [string]$OverrideVariable
    )

    if (-not [string]::IsNullOrWhiteSpace($OverridePath)) {
        $resolvedOverridePath = $OverridePath
        if (Test-Path -LiteralPath $resolvedOverridePath) {
            $resolvedOverridePath = (Resolve-Path -LiteralPath $resolvedOverridePath).Path
        } else {
            throw "$ToolName not found: $resolvedOverridePath ($OverrideVariable)"
        }

        return [pscustomobject]@{
            Path = $resolvedOverridePath
            VersionLabel = Get-MplabXVersionLabelFromPath -Path $resolvedOverridePath
            Source = $OverrideVariable
        }
    }

    $toolMatches = @(Get-MplabXInstallRoots |
        ForEach-Object {
            $toolPath = Join-Path $_.Path $RelativePath
            if (Test-Path -LiteralPath $toolPath) {
                [pscustomobject]@{
                    Path = $toolPath
                    Version = $_.Version
                    VersionLabel = $_.VersionLabel
                    Source = $_.Path
                }
            }
        } |
        Sort-Object -Property Version -Descending)

    if ($toolMatches.Count -eq 0) {
        throw "$ToolName not found. Install MPLAB X or set $OverrideVariable."
    }

    return $toolMatches[0]
}

function Remove-ProjectRootIntermediates {
    param(
        [string]$ProjectDir
    )

    $patterns = @(
        '*.d',
        '*.i',
        '*.s',
        '*.o',
        '*.obj',
        '*.lst',
        '*.map',
        '*.elf',
        '*.hex',
        '*.hxl',
        '*.cof',
        'p33*MPS*.*.00'
    )
    $removedCount = 0
    $lockedFiles = @()

    foreach ($pattern in $patterns) {
        Get-ChildItem -LiteralPath $ProjectDir -File -Filter $pattern -Force -ErrorAction SilentlyContinue |
            ForEach-Object {
                $path = $_.FullName
                try {
                    Remove-Item -LiteralPath $path -Force -ErrorAction Stop
                    $removedCount++
                } catch {
                    $lockedFiles += $path
                }
            }
    }

    if ($lockedFiles.Count -gt 0) {
        Write-Host "WARNING: $($lockedFiles.Count) intermediate file(s) still locked in project root:"
        $lockedFiles | ForEach-Object { Write-Host "  $_" }
        throw "Clean incomplete: locked project-root intermediate file(s)."
    }

    Write-Host "cleaned project-root intermediate files: $removedCount"
}

function Remove-GeneratedMakefiles {
    param(
        [string]$ProjectDir
    )

    $nbprojectDir = Join-Path $ProjectDir 'nbproject'
    $patterns = @(
        'Makefile-dsPIC33AK*.mk',
        'Makefile-local-*.mk',
        'Makefile-genesis.properties'
    )
    $removedCount = 0
    $lockedFiles = @()

    foreach ($pattern in $patterns) {
        Get-ChildItem -LiteralPath $nbprojectDir -File -Filter $pattern -Force -ErrorAction SilentlyContinue |
            ForEach-Object {
                $path = $_.FullName
                try {
                    Remove-Item -LiteralPath $path -Force -ErrorAction Stop
                    $removedCount++
                } catch {
                    $lockedFiles += $path
                }
            }
    }

    if ($lockedFiles.Count -gt 0) {
        Write-Host "WARNING: $($lockedFiles.Count) generated makefile(s) still locked:"
        $lockedFiles | ForEach-Object { Write-Host "  $_" }
        throw "Clean incomplete: locked generated makefile(s)."
    }

    Write-Host "cleaned generated makefiles: $removedCount"
}

function Invoke-ConfigurationClean {
    param(
        [Parameter(Mandatory = $true)]
        [string]$CleanScript,
        [Parameter(Mandatory = $true)]
        [string]$Configuration
    )

    # clean.ps1 is also used directly by VS Code and therefore reads its target
    # from MPLABX_CONF. Scope that env var to just this call (save/restore) so a
    # build.ps1 -Configuration override cannot leak into a later unqualified
    # clean.ps1/build.ps1 invocation in the same shell.
    $hadPrevious = Test-Path Env:MPLABX_CONF
    $previous = $env:MPLABX_CONF
    try {
        $env:MPLABX_CONF = $Configuration
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $CleanScript
    }
    finally {
        if ($hadPrevious) {
            $env:MPLABX_CONF = $previous
        }
        else {
            Remove-Item Env:MPLABX_CONF -ErrorAction SilentlyContinue
        }
    }
}

$repoRoot = Resolve-HalStarterRepoRoot -RequestedRoot $Root
$projectDir = Resolve-HalStarterProjectDir -RepoRoot $repoRoot -RequestedProjectDir $ProjectDir
$configurations = Get-HalStarterConfigurations -ProjectDir $projectDir
$presetCatalog = Get-HalStarterPresetCatalog -RepoRoot $repoRoot
$cleanScript = Join-Path $repoRoot '.vscode\clean.ps1'

if ([string]::IsNullOrWhiteSpace($Configuration)) {
    $Configuration = Get-HalStarterActiveConfiguration -ProjectDir $projectDir -Configurations $configurations
}
$configurationEntry = Get-HalStarterConfiguration -Configurations $configurations -Name $Configuration

# --- What APP_BUILD variation is this build? --------------------------------
# -Preset wins; otherwise the persisted selection for this configuration; only
# if there is none does the build fall back to the header's own default (no
# -DAPP_BUILD passed at all, which is what an MPLAB X IDE build does).
$selectedAppPreset = $null
$presetSource = $null

if (-not [string]::IsNullOrWhiteSpace($Preset)) {
    $presetEntry = Get-HalStarterPreset -Catalog $presetCatalog -Name $Preset
    if ($null -eq $presetEntry) {
        $allowed = ($presetCatalog.Presets | ForEach-Object { $_.Name }) -join ', '
        throw "Unknown APP_BUILD variation '$Preset'. Known variations: $allowed"
    }
    $selectedAppPreset = $presetEntry.Name
    $presetSource = '-Preset'
}

if ($null -eq $selectedAppPreset) {
    $persistedPreset = Get-HalStarterSelectedPreset -RepoRoot $repoRoot -Configuration $Configuration
    if ($persistedPreset) {
        $persistedEntry = Get-HalStarterPreset -Catalog $presetCatalog -Name $persistedPreset
        if ($null -eq $persistedEntry) {
            Write-Host "WARNING: ignoring stored APP_BUILD '$persistedPreset' - not a known variation. Re-run ./buildtools/switch_config.ps1."
        } else {
            $selectedAppPreset = $persistedEntry.Name
            $presetSource = 'active selection'
        }
    }
}

$configurationDefaultPreset = Get-HalStarterDefaultPreset -Catalog $presetCatalog
# Identity of this build's APP_BUILD, used as the build-directory stamp. Without
# an explicit variation the compiler picks the header's own default, so record
# that as the stamp instead of a variation name.
$appBuildIdentity = if ($null -ne $selectedAppPreset) { $selectedAppPreset } else { "(default:$configurationDefaultPreset)" }

$makefile = Join-Path $projectDir "nbproject\Makefile-$Configuration.mk"

if (-not (Test-Path -LiteralPath $projectDir)) {
    throw "MPLAB X project directory not found: $projectDir"
}
$modeCount = @($Full, $Clean, $Generate) | Where-Object { $_ } | Measure-Object | Select-Object -ExpandProperty Count
if ($modeCount -gt 1) {
    throw "Use only one of -Full, -Clean, or -Generate."
}

# The APP_BUILD variations within this configuration share one object directory:
# objects built with another variation must never be reused. The build directory
# carries a stamp of what it was built with, so only a real change forces a
# clean build - rebuilding the same variation stays incremental.
$builtAppBuild = Get-HalStarterBuiltPreset -ProjectDir $projectDir -Configuration $Configuration
$buildDirExists = Test-Path -LiteralPath (Get-HalStarterConfigurationBuildDir -ProjectDir $projectDir -Configuration $Configuration)
if (-not ($Full -or $Clean -or $Generate) -and $buildDirExists) {
    $promoteReason = $null
    if ($null -eq $builtAppBuild) {
        $promoteReason = 'APP_BUILD of the existing objects is unknown (built outside build.ps1?)'
    } elseif ($builtAppBuild -ne $appBuildIdentity) {
        $promoteReason = "APP_BUILD changed ($builtAppBuild -> $appBuildIdentity)"
    }
    if ($null -ne $promoteReason) {
        Write-Host "${promoteReason}: promoting to -Full."
        $Full = $true
    }
}

Write-Host "Root: $repoRoot"
Write-Host "Project: $projectDir"
Write-Host "Configuration: $Configuration  ($($configurationEntry.Device))"
if ($null -ne $selectedAppPreset) {
    Write-Host "APP_BUILD: $selectedAppPreset  [$presetSource]"
} else {
    Write-Host "APP_BUILD: $configurationDefaultPreset  [compile-time default; none selected]"
}

Push-Location $projectDir
try {
    if ($Clean) {
        if (-not (Test-Path -LiteralPath $cleanScript)) {
            throw "clean script not found: $cleanScript"
        }

        Invoke-CheckedCommand -Description "Clean $Configuration outputs" -Command {
            Invoke-ConfigurationClean -CleanScript $cleanScript -Configuration $Configuration
        }
        Remove-ProjectRootIntermediates -ProjectDir $projectDir
        Remove-GeneratedMakefiles -ProjectDir $projectDir
        return
    }

    if ($Generate) {
        $generatorTool = Resolve-MplabXTool `
            -ToolName 'prjMakefilesGenerator.bat' `
            -RelativePath 'mplab_platform\bin\prjMakefilesGenerator.bat' `
            -OverridePath $env:MPLABX_GEN `
            -OverrideVariable 'MPLABX_GEN'
        Write-Host "MPLAB X generator: $($generatorTool.VersionLabel) ($($generatorTool.Path))"

        Invoke-CheckedCommand -Description 'Generate MPLAB X makefiles' -Command {
            & $($generatorTool.Path) '.'
        }
        return
    }

    $makeTool = Resolve-MplabXTool `
        -ToolName 'make.exe' `
        -RelativePath 'gnuBins\GnuWin32\bin\make.exe' `
        -OverridePath $env:MPLABX_MAKE `
        -OverrideVariable 'MPLABX_MAKE'
    Write-Host "MPLAB X make: $($makeTool.VersionLabel) ($($makeTool.Path))"

    $needsMakefileGeneration = $Full -or -not (Test-Path -LiteralPath $makefile)
    if ($needsMakefileGeneration) {
        $generatorTool = Resolve-MplabXTool `
            -ToolName 'prjMakefilesGenerator.bat' `
            -RelativePath 'mplab_platform\bin\prjMakefilesGenerator.bat' `
            -OverridePath $env:MPLABX_GEN `
            -OverrideVariable 'MPLABX_GEN'
        Write-Host "MPLAB X generator: $($generatorTool.VersionLabel) ($($generatorTool.Path))"

        Invoke-CheckedCommand -Description 'Generate MPLAB X makefiles' -Command {
            & $($generatorTool.Path) '.'
        }
    }

    if ($Full) {
        if (-not (Test-Path -LiteralPath $cleanScript)) {
            throw "clean script not found: $cleanScript"
        }

        Invoke-CheckedCommand -Description "Clean $Configuration outputs" -Command {
            Invoke-ConfigurationClean -CleanScript $cleanScript -Configuration $Configuration
        }
        Remove-ProjectRootIntermediates -ProjectDir $projectDir
    }

    $extraDefines = @()
    if ($null -ne $selectedAppPreset) {
        $extraDefines += "-DAPP_BUILD=$selectedAppPreset"
        # One preset (APP_BUILD_TDM_NEG_TEST_2LEG) also needs a HAL-layer macro.
        # That macro is intentionally not set by app_build_config.h (app-layer)
        # itself -- see hal_starter_build_state.ps1's HalStarterPresetExtraDefines
        # comment -- so inject it here on the compiler command line instead.
        foreach ($extra in (Get-HalStarterPresetExtraDefines -PresetName $selectedAppPreset)) {
            $extraDefines += "-D$extra"
        }
    }

    $buildCommand = {
        & $($makeTool.Path) "-j$Jobs" -f "nbproject/Makefile-$Configuration.mk" SUBPROJECTS= .build-conf
    }
    if ($extraDefines.Count -gt 0) {
        $extraCcPre = "MP_EXTRA_CC_PRE=$($extraDefines -join ' ')"
        $buildCommand = {
            & $($makeTool.Path) "-j$Jobs" -f "nbproject/Makefile-$Configuration.mk" SUBPROJECTS= $extraCcPre .build-conf
        }
    }

    Invoke-CheckedCommand -Description "Build $Configuration (-j$Jobs)" -Command $buildCommand

    # Record what these objects were built with, so the next build knows whether
    # it can be incremental (see the promotion block above) and flashauto.ps1 can
    # report which variation the HEX came from.
    Set-HalStarterBuiltPreset -ProjectDir $projectDir -Configuration $Configuration -Value $appBuildIdentity

    # A successful compiler/linker run is not yet a complete dual-partition release:
    # the initial PKOB4 image must provision both physical UCA copies, and the
    # serial updater needs a partition-agnostic DBFW package. Generate and verify
    # both artifacts automatically so a beginner has one build command.
    $projectName = Split-Path -Leaf $projectDir
    $productionDir = Join-Path $projectDir "dist\$Configuration\production"
    $productionHex = Join-Path $productionDir "$projectName.production.hex"
    $reflashImage = Join-Path $productionDir 'reflash_image.bin'
    $provisionScript = Join-Path $repoRoot 'buildtools\provision.ps1'
    $extractScript = Join-Path $repoRoot 'tools\extract_p1_image.py'

    if (-not (Test-Path -LiteralPath $productionHex)) {
        throw "Build succeeded but production HEX is missing: $productionHex"
    }
    foreach ($required in @($provisionScript, $extractScript)) {
        if (-not (Test-Path -LiteralPath $required)) {
            throw "Post-build tool not found: $required"
        }
    }

    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
    if ($null -eq $pythonCommand) {
        $pythonCommand = Get-Command py -ErrorAction SilentlyContinue
    }
    if ($null -eq $pythonCommand) {
        throw 'Python 3 was not found on PATH; it is required to generate verified dual-partition artifacts.'
    }
    $pythonExe = $pythonCommand.Source

    Write-Host "==> Generate + verify dual-partition provisioning bundle"
    & $provisionScript -Configuration $Configuration -Root $repoRoot `
        -ProjectDir $projectDir -Hex $productionHex -Python $pythonExe

    Invoke-CheckedCommand -Description 'Generate reflash_image.bin' -Command {
        & $pythonExe $extractScript $productionHex $reflashImage
    }

    Write-Host ''
    Write-Host 'Dual-partition artifacts: PASS'
    Write-Host "  APP_BUILD     : $appBuildIdentity"
    Write-Host "  initial flash : $($productionHex -replace '\.hex$', '.bundle.hex')"
    Write-Host "  XMODEM image  : $reflashImage"
}
finally {
    Pop-Location
}
