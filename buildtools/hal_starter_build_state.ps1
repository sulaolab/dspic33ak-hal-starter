# hal_starter_build_state.ps1 - shared build-selection state for the buildtools scripts.
#
# Dot-sourced by switch_config.ps1 / build.ps1 / flashauto.ps1. Ported from an
# internal multi-configuration project and simplified for this repo's single
# MPLAB configuration / single application:
#
#   1. WHICH MPLAB CONFIGURATION (= device)
#      Catalog : firmware.X/nbproject/configurations.xml (conf order = the
#                <defaultConf> index, targetDevice = the flash device token)
#      Active  : nbproject/Makefile-impl.mk  DEFAULTCONF=<name>   (command line)
#                nbproject/private/configurations.xml <defaultConf>N</defaultConf> (IDE)
#      Both active files are byte-exact CRLF; only the one value is rewritten.
#      Today there is exactly one configuration; this plumbing is correct ahead
#      of time so a future board variant is a catalog addition, not a rewrite.
#
#   2. WHICH APP_BUILD VARIATION (preset)
#      Catalog : src/app/app_build_config.h (names, one-line detail text via the
#                boot-banner APP_BUILD_NAME/APP_BUILD_DETAIL pair, and the
#                compile-time default from its #ifndef APP_BUILD block)
#      Active  : buildtools/active_build.json (untracked)
#      Stamp   : firmware.X/build/<conf>/.hal_starter_app_build records the
#                APP_BUILD the existing objects were compiled with, so a build
#                only has to clean when the variation actually changed.
#
# There is deliberately NO environment variable in this path: MPLABX_CONF as a
# *default source* leaks across a shell session (one explicit -Configuration
# silently affects every later unqualified build). build.ps1 still sets
# $env:MPLABX_CONF around the existing clean.ps1 call, which is the one place that
# script still reads it.

$ErrorActionPreference = 'Stop'

$script:HalStarterStateFileRelative = 'buildtools\active_build.json'
$script:HalStarterBuildStampName = '.hal_starter_app_build'
$script:HalStarterAppBuildHeaderRelative = 'src\app\app_build_config.h'

# One preset needs a HAL-layer macro that must NOT come from app_build_config.h
# (that header is app-layer; dspic33ak_spi_i2s_tdm_conf.h, the HAL's own config,
# documents that it must never gain an app-layer dependency). build.ps1 injects
# these directly on the compiler command line for the listed preset only.
$script:HalStarterPresetExtraDefines = @{
    'APP_BUILD_TDM_NEG_TEST_2LEG' = @('DSPIC33AK_TDM_USE_SPI2=1')
}

function Resolve-HalStarterRepoRoot {
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

function Resolve-HalStarterProjectDir {
    param(
        [string]$RepoRoot,
        [string]$RequestedProjectDir
    )

    if (-not [string]::IsNullOrWhiteSpace($RequestedProjectDir)) {
        if ([System.IO.Path]::IsPathRooted($RequestedProjectDir)) {
            return (Resolve-Path -LiteralPath $RequestedProjectDir).Path
        }
        return (Resolve-Path -LiteralPath (Join-Path $RepoRoot $RequestedProjectDir)).Path
    }

    $projects = @(Get-ChildItem -LiteralPath $RepoRoot -Directory -Filter '*.X' |
        Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'nbproject') })

    if ($projects.Count -eq 0) {
        throw "No MPLAB X project directory (*.X with nbproject) found under: $RepoRoot"
    }
    if ($projects.Count -gt 1) {
        $names = ($projects | ForEach-Object { $_.Name }) -join ', '
        throw "Multiple MPLAB X project directories found: $names. Specify -ProjectDir."
    }

    return $projects[0].FullName
}

function Get-HalStarterConfigurations {
    param(
        [string]$ProjectDir
    )

    $configurationsXml = Join-Path $ProjectDir 'nbproject\configurations.xml'
    if (-not (Test-Path -LiteralPath $configurationsXml)) {
        throw "MPLAB configuration catalog not found: $configurationsXml"
    }

    [xml]$xml = Get-Content -LiteralPath $configurationsXml -Raw
    $confNodes = @($xml.configurationDescriptor.confs.conf)
    if ($confNodes.Count -eq 0) {
        throw "No <conf> entries found in $configurationsXml"
    }

    $result = @()
    for ($i = 0; $i -lt $confNodes.Count; $i++) {
        $node = $confNodes[$i]
        $result += [pscustomobject]@{
            Name   = $node.name
            Index  = $i
            Device = $node.toolsSet.targetDevice
        }
    }

    return $result
}

function Get-HalStarterConfiguration {
    param(
        [object[]]$Configurations,
        [string]$Name
    )

    $match = @($Configurations | Where-Object { $_.Name -eq $Name })
    if ($match.Count -ne 1) {
        $available = ($Configurations | ForEach-Object { $_.Name }) -join ', '
        throw "Unknown MPLAB configuration '$Name'. Available: $available"
    }

    return $match[0]
}

function Get-HalStarterActiveConfiguration {
    param(
        [string]$ProjectDir,
        [object[]]$Configurations
    )

    $implMakefile = Join-Path $ProjectDir 'nbproject\Makefile-impl.mk'
    if (Test-Path -LiteralPath $implMakefile) {
        $text = [System.IO.File]::ReadAllText($implMakefile, [System.Text.Encoding]::ASCII)
        $match = [regex]::Match($text, '(?m)^DEFAULTCONF=([^\s\r\n]+)')
        if ($match.Success) {
            $name = $match.Groups[1].Value
            if (@($Configurations | Where-Object { $_.Name -eq $name }).Count -eq 1) {
                return $name
            }
        }
    }

    return $Configurations[0].Name
}

function Update-HalStarterAsciiFileExact {
    <#
      Byte-level single-value rewrite. Makefile-impl.mk and
      private/configurations.xml are CRLF byte-exact, so Get-Content/Set-Content
      must not be used: they would renormalize every line and turn a one-value
      switch into a whole-file EOL diff.
    #>
    param(
        [string]$Path,
        [string]$Pattern,
        [scriptblock]$Replacement
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "File not found: $Path"
    }

    $encoding = [System.Text.Encoding]::GetEncoding(28591)
    $bytes = [System.IO.File]::ReadAllBytes($Path)
    $text = $encoding.GetString($bytes)
    $regex = [System.Text.RegularExpressions.Regex]::new($Pattern, [System.Text.RegularExpressions.RegexOptions]::Multiline)
    $found = $regex.Matches($text)
    if ($found.Count -ne 1) {
        throw "Expected exactly one match in $Path, found $($found.Count)"
    }

    $newText = $regex.Replace(
        $text,
        {
            param($match)
            & $Replacement $match
        },
        1)

    if ($newText -ne $text) {
        [System.IO.File]::WriteAllBytes($Path, $encoding.GetBytes($newText))
    }
}

function Set-HalStarterActiveConfiguration {
    param(
        [string]$ProjectDir,
        [object]$ConfigurationEntry
    )

    $name = $ConfigurationEntry.Name
    $index = [string]$ConfigurationEntry.Index

    $implMakefile = Join-Path $ProjectDir 'nbproject\Makefile-impl.mk'
    $privateConfig = Join-Path $ProjectDir 'nbproject\private\configurations.xml'

    # Some projects track Makefile-impl.mk in git so it always exists; this repo's
    # .gitignore instead excludes every generated nbproject/Makefile-*.mk, including
    # this one -- a fresh clone has none of them until the first
    # build.ps1/-Generate run or an MPLAB X IDE open. With
    # a single configuration there is nothing to rewrite yet in that case: the
    # generator will stamp the only choice as DEFAULTCONF regardless.
    if (Test-Path -LiteralPath $implMakefile) {
        Update-HalStarterAsciiFileExact `
            -Path $implMakefile `
            -Pattern '^(DEFAULTCONF=)[^\r\n]*' `
            -Replacement { param($m) $m.Groups[1].Value + $name }
    }

    # Untracked IDE state: absent until MPLAB X has opened the project once.
    if (Test-Path -LiteralPath $privateConfig) {
        Update-HalStarterAsciiFileExact `
            -Path $privateConfig `
            -Pattern '(<defaultConf>)\d+(</defaultConf>)' `
            -Replacement { param($m) $m.Groups[1].Value + $index + $m.Groups[2].Value }
    }
}

function Get-HalStarterPresetCatalog {
    <#
      Parses src/app/app_build_config.h - the single place that owns the
      APP_BUILD variation set - and returns:
        Presets : ordered list of @{ Name; Value; Detail }
        Default : the header's #ifndef APP_BUILD compile-time default name
      Adding a variation to the header is therefore enough; no list here to
      update.
    #>
    param(
        [string]$RepoRoot
    )

    $headerPath = Join-Path $RepoRoot $script:HalStarterAppBuildHeaderRelative
    if (-not (Test-Path -LiteralPath $headerPath)) {
        throw "APP_BUILD catalog header not found: $headerPath"
    }

    $text = [System.IO.File]::ReadAllText($headerPath)

    # 1) numeric variation defines: #define APP_BUILD_FOO (7)
    $values = [ordered]@{}
    foreach ($m in [regex]::Matches($text, '(?m)^\s*#define\s+(APP_BUILD_[A-Z0-9_]+)\s+\((\d+)\)')) {
        $values[$m.Groups[1].Value] = [int]$m.Groups[2].Value
    }
    if ($values.Count -eq 0) {
        throw "No '#define APP_BUILD_* (n)' variations found in $headerPath"
    }

    # 2) one-line description, taken from the boot-banner APP_BUILD_NAME /
    #    APP_BUILD_DETAIL pairs.
    $details = @{}
    $nameMatches = @([regex]::Matches($text, '#define\s+APP_BUILD_NAME\s+"(APP_BUILD_\w+)"'))
    for ($n = 0; $n -lt $nameMatches.Count; $n++) {
        $start = $nameMatches[$n].Index + $nameMatches[$n].Length
        $end = if ($n + 1 -lt $nameMatches.Count) { $nameMatches[$n + 1].Index } else { $text.Length }
        $segment = $text.Substring($start, $end - $start)
        $detailMatches = @([regex]::Matches($segment, '#define\s+APP_BUILD_DETAIL\s+"([^"]*)"'))
        if ($detailMatches.Count -gt 0) {
            $details[$nameMatches[$n].Groups[1].Value] = $detailMatches[$detailMatches.Count - 1].Groups[1].Value
        }
    }

    # 3) compile-time default (#ifndef APP_BUILD block)
    $defaultMatch = [regex]::Match(
        $text,
        '#ifndef\s+APP_BUILD\s*\r?\n\s*#define\s+APP_BUILD\s+\((APP_BUILD_\w+)\)')
    if (-not $defaultMatch.Success) {
        throw "Could not determine the compile-time default APP_BUILD in $headerPath"
    }
    $defaultName = $defaultMatch.Groups[1].Value
    if (-not $values.Contains($defaultName)) {
        throw "Default APP_BUILD '$defaultName' is not one of the catalog's #define entries in $headerPath"
    }

    $presets = @()
    foreach ($name in $values.Keys) {
        $presets += [pscustomobject]@{
            Name   = $name
            Value  = $values[$name]
            Detail = if ($details.ContainsKey($name)) { $details[$name] } else { '' }
        }
    }

    return [pscustomobject]@{
        Presets    = @($presets | Sort-Object -Property Value)
        Default    = $defaultName
        HeaderPath = $headerPath
    }
}

function Get-HalStarterPreset {
    param(
        [object]$Catalog,
        [string]$Name
    )

    $match = @($Catalog.Presets | Where-Object { $_.Name -eq $Name })
    if ($match.Count -ne 1) {
        return $null
    }

    return $match[0]
}

function Get-HalStarterDefaultPreset {
    param(
        [object]$Catalog
    )

    return $Catalog.Default
}

function Get-HalStarterPresetExtraDefines {
    param(
        [string]$PresetName
    )

    if ($script:HalStarterPresetExtraDefines.ContainsKey($PresetName)) {
        return @($script:HalStarterPresetExtraDefines[$PresetName])
    }

    return @()
}

function Get-HalStarterStateFilePath {
    param(
        [string]$RepoRoot
    )

    return (Join-Path $RepoRoot $script:HalStarterStateFileRelative)
}

function Get-HalStarterSelectedPresets {
    param(
        [string]$RepoRoot
    )

    $statePath = Get-HalStarterStateFilePath -RepoRoot $RepoRoot
    if (-not (Test-Path -LiteralPath $statePath)) {
        return @{}
    }

    try {
        $json = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
    } catch {
        Write-Host "WARNING: ignoring unreadable $statePath ($($_.Exception.Message))"
        return @{}
    }

    $map = @{}
    if ($null -ne $json -and $null -ne $json.presets) {
        foreach ($property in $json.presets.PSObject.Properties) {
            $map[$property.Name] = [string]$property.Value
        }
    }

    return $map
}

function Get-HalStarterSelectedPreset {
    param(
        [string]$RepoRoot,
        [string]$Configuration
    )

    $map = Get-HalStarterSelectedPresets -RepoRoot $RepoRoot
    if ($map.ContainsKey($Configuration)) {
        return $map[$Configuration]
    }

    return $null
}

function Set-HalStarterSelectedPreset {
    param(
        [string]$RepoRoot,
        [string]$Configuration,
        [string]$Preset
    )

    $map = Get-HalStarterSelectedPresets -RepoRoot $RepoRoot
    $map[$Configuration] = $Preset

    $presets = [ordered]@{}
    foreach ($key in ($map.Keys | Sort-Object)) {
        $presets[$key] = $map[$key]
    }

    $payload = [ordered]@{
        comment = 'Local build selection written by buildtools/switch_config.ps1. Untracked; delete to fall back to the compile-time default.'
        presets = $presets
    }

    $statePath = Get-HalStarterStateFilePath -RepoRoot $RepoRoot
    $json = ($payload | ConvertTo-Json -Depth 4)
    $json = $json -replace '(?<!\r)\n', "`r`n"
    [System.IO.File]::WriteAllText($statePath, $json + "`r`n", [System.Text.UTF8Encoding]::new($false))
}

function Get-HalStarterConfigurationBuildDir {
    param(
        [string]$ProjectDir,
        [string]$Configuration
    )

    return (Join-Path $ProjectDir "build\$Configuration")
}

function Get-HalStarterBuiltPreset {
    <#
      What the objects currently in build/<conf>/ were compiled with:
        <name>     an explicit -DAPP_BUILD=<name> build
        (default)  built without -DAPP_BUILD (compile-time default)
        $null      nothing built, or built by something that leaves no stamp
                   (MPLAB X IDE), which callers must treat as "unknown".
    #>
    param(
        [string]$ProjectDir,
        [string]$Configuration
    )

    $stampPath = Join-Path (Get-HalStarterConfigurationBuildDir -ProjectDir $ProjectDir -Configuration $Configuration) $script:HalStarterBuildStampName
    if (-not (Test-Path -LiteralPath $stampPath)) {
        return $null
    }

    $value = ([System.IO.File]::ReadAllText($stampPath)).Trim()
    if ([string]::IsNullOrWhiteSpace($value)) {
        return $null
    }

    return $value
}

function Set-HalStarterBuiltPreset {
    param(
        [string]$ProjectDir,
        [string]$Configuration,
        [string]$Value
    )

    $buildDir = Get-HalStarterConfigurationBuildDir -ProjectDir $ProjectDir -Configuration $Configuration
    if (-not (Test-Path -LiteralPath $buildDir)) {
        # No build tree (e.g. a build that produced nothing): nothing to stamp.
        return
    }

    $stampPath = Join-Path $buildDir $script:HalStarterBuildStampName
    [System.IO.File]::WriteAllText($stampPath, $Value + "`r`n", [System.Text.UTF8Encoding]::new($false))
}
