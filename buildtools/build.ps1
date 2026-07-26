param(
    [switch]$Full,
    [switch]$Clean,
    [switch]$Generate,
    [string]$Configuration = $env:MPLABX_CONF,
    [string]$Root = (Get-Location).Path,
    [string]$ProjectDir
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($Configuration)) {
    $Configuration = 'dsPIC33AK512'
}

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

function Resolve-BuildRoot {
    param(
        [string]$RequestedRoot
    )

    $resolvedRoot = (Resolve-Path -LiteralPath $RequestedRoot).Path

    if ((Split-Path -Leaf $resolvedRoot) -like '*.X' -and
        (Test-Path -LiteralPath (Join-Path $resolvedRoot 'nbproject'))) {
        return (Split-Path -Parent $resolvedRoot)
    }

    return $resolvedRoot
}

function Resolve-MplabProjectDir {
    param(
        [string]$Root,
        [string]$RequestedProjectDir
    )

    if (-not [string]::IsNullOrWhiteSpace($RequestedProjectDir)) {
        if ([System.IO.Path]::IsPathRooted($RequestedProjectDir)) {
            return (Resolve-Path -LiteralPath $RequestedProjectDir).Path
        }
        return (Resolve-Path -LiteralPath (Join-Path $Root $RequestedProjectDir)).Path
    }

    $projects = @(Get-ChildItem -LiteralPath $Root -Directory -Filter '*.X' |
        Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'nbproject') })

    if ($projects.Count -eq 0) {
        throw "No MPLAB X project directory (*.X with nbproject) found under: $Root"
    }
    if ($projects.Count -gt 1) {
        $names = ($projects | ForEach-Object { $_.Name }) -join ', '
        throw "Multiple MPLAB X project directories found: $names. Specify -ProjectDir."
    }

    return $projects[0].FullName
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

$repoRoot = Resolve-BuildRoot -RequestedRoot $Root
$projectDir = Resolve-MplabProjectDir -Root $repoRoot -RequestedProjectDir $ProjectDir
$makefile = Join-Path $projectDir "nbproject\Makefile-$Configuration.mk"
$cleanScript = Join-Path $repoRoot '.vscode\clean.ps1'

if (-not (Test-Path -LiteralPath $projectDir)) {
    throw "MPLAB X project directory not found: $projectDir"
}
$modeCount = @($Full, $Clean, $Generate) | Where-Object { $_ } | Measure-Object | Select-Object -ExpandProperty Count
if ($modeCount -gt 1) {
    throw "Use only one of -Full, -Clean, or -Generate."
}

$env:MPLABX_CONF = $Configuration
Write-Host "Root: $repoRoot"
Write-Host "Project: $projectDir"
Write-Host "Configuration: $Configuration"

Push-Location $projectDir
try {
    if ($Clean) {
        if (-not (Test-Path -LiteralPath $cleanScript)) {
            throw "clean script not found: $cleanScript"
        }

        Invoke-CheckedCommand -Description "Clean $Configuration outputs" -Command {
            & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $cleanScript
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
            & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $cleanScript
        }
        Remove-ProjectRootIntermediates -ProjectDir $projectDir
    }

    Invoke-CheckedCommand -Description "Build $Configuration" -Command {
        & $($makeTool.Path) -f "nbproject/Makefile-$Configuration.mk" SUBPROJECTS= .build-conf
    }

    # A successful compiler/linker run is not yet a complete Dual Bank release:
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
        throw 'Python 3 was not found on PATH; it is required to generate verified Dual Bank artifacts.'
    }
    $pythonExe = $pythonCommand.Source

    Write-Host "==> Generate + verify dual-partition provisioning bundle"
    & $provisionScript -Configuration $Configuration -Root $repoRoot `
        -ProjectDir $projectDir -Hex $productionHex -Python $pythonExe

    Invoke-CheckedCommand -Description 'Generate reflash_image.bin' -Command {
        & $pythonExe $extractScript $productionHex $reflashImage
    }

    Write-Host ''
    Write-Host 'Dual Bank artifacts: PASS'
    Write-Host "  initial flash : $($productionHex -replace '\.hex$', '.bundle.hex')"
    Write-Host "  XMODEM image  : $reflashImage"
}
finally {
    Pop-Location
}
