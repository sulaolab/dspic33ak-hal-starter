# switch_config.ps1 - select what the next build targets.
#
# Two things are selected, and both persist so a bare build.ps1 / flashauto.ps1
# follows them (MPLAB-X-like "active configuration" behaviour, no environment
# variable involved):
#
#   1. the MPLAB configuration  -> device (today: dsPIC33AK512 only)
#      stored in the project itself, exactly where MPLAB X IDE stores it
#   2. the APP_BUILD variation  -> which demo variant is compiled
#      stored in buildtools/active_build.json (untracked)
#
# Run with no arguments for the interactive menu; pass -Configuration / -Preset
# to script it. See buildtools/README.md.

param(
    # MPLAB configuration name, e.g. dsPIC33AK512.
    [string]$Configuration,
    # APP_BUILD variation, e.g. APP_BUILD_STARTER_DEFAULT / APP_BUILD_TDM_SMOKE_OFF.
    [string]$Preset,
    # Print the catalog and the current selection, change nothing.
    [switch]$List,
    [string]$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path,
    [string]$ProjectDir
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'hal_starter_build_state.ps1')

function Write-ConfigurationCatalog {
    param(
        [object[]]$Configurations,
        [string]$ActiveName
    )

    Write-Host 'MPLAB configurations:'
    for ($i = 0; $i -lt $Configurations.Count; $i++) {
        $entry = $Configurations[$i]
        $marker = if ($entry.Name -eq $ActiveName) { '  <- active' } else { '' }
        Write-Host ("  {0}) {1,-20} {2}{3}" -f ($i + 1), $entry.Name, $entry.Device, $marker)
    }
}

function Write-PresetCatalog {
    param(
        [object[]]$Presets,
        [string]$SelectedName,
        [string]$DefaultName
    )

    for ($i = 0; $i -lt $Presets.Count; $i++) {
        $entry = $Presets[$i]
        $tags = @()
        if ($entry.Name -eq $SelectedName) { $tags += 'selected' }
        if ($entry.Name -eq $DefaultName) { $tags += 'compile-time default' }
        $marker = if ($tags.Count -gt 0) { "  <- $($tags -join ', ')" } else { '' }
        Write-Host ("  {0,2}) {1,-38} {2}" -f ($i + 1), $entry.Name, $entry.Detail)
        if ($marker) { Write-Host ("      {0}" -f $marker.TrimStart()) }
    }
}

function Read-MenuChoice {
    <#
      Returns the 1-based index chosen, 0 for "keep current" (Enter), or $null
      for quit. Loops until the answer is valid.
    #>
    param(
        [string]$Prompt,
        [int]$Count
    )

    while ($true) {
        $answer = Read-Host $Prompt
        if ($null -eq $answer) { return $null }
        $answer = $answer.Trim()

        if ($answer -eq '') { return 0 }
        if ($answer -in @('q', 'Q', 'quit', 'exit')) { return $null }

        $index = 0
        if ([int]::TryParse($answer, [ref]$index) -and $index -ge 1 -and $index -le $Count) {
            return $index
        }

        Write-Host "  Enter 1-$Count, Enter to keep, or q to quit."
    }
}

$repoRoot = Resolve-HalStarterRepoRoot -RequestedRoot $Root
$projectDir = Resolve-HalStarterProjectDir -RepoRoot $repoRoot -RequestedProjectDir $ProjectDir
$configurations = Get-HalStarterConfigurations -ProjectDir $projectDir
$catalog = Get-HalStarterPresetCatalog -RepoRoot $repoRoot

$activeName = Get-HalStarterActiveConfiguration -ProjectDir $projectDir -Configurations $configurations
$interactive = [string]::IsNullOrWhiteSpace($Configuration) -and
               [string]::IsNullOrWhiteSpace($Preset) -and
               -not $List

# ---------------------------------------------------------------- list only ---
if ($List) {
    $selectedPreset = Get-HalStarterSelectedPreset -RepoRoot $repoRoot -Configuration $activeName
    $defaultPreset = Get-HalStarterDefaultPreset -Catalog $catalog

    Write-Host "Project: $projectDir"
    Write-Host ''
    Write-ConfigurationCatalog -Configurations $configurations -ActiveName $activeName
    Write-Host ''
    Write-Host 'APP_BUILD variations:'
    Write-PresetCatalog `
        -Presets $catalog.Presets `
        -SelectedName $selectedPreset `
        -DefaultName $defaultPreset
    Write-Host ''
    Write-Host "Active configuration: $activeName"
    if ($selectedPreset) {
        Write-Host "Active APP_BUILD:     $selectedPreset"
    } else {
        Write-Host "Active APP_BUILD:     (none selected -> compile-time default $defaultPreset)"
    }
    return
}

# ------------------------------------------------------------- interactive ----
if ($interactive) {
    if ([Console]::IsInputRedirected) {
        throw @'
switch_config.ps1 needs a console for its interactive menu (stdin is redirected).
Pass the selection explicitly instead, for example:
  ./buildtools/switch_config.ps1 -Configuration dsPIC33AK512 -Preset APP_BUILD_TDM_SMOKE_OFF
  ./buildtools/switch_config.ps1 -List
'@
    }

    Write-Host "Project: $projectDir"
    Write-Host ''
    Write-ConfigurationCatalog -Configurations $configurations -ActiveName $activeName
    Write-Host ''
    $choice = Read-MenuChoice `
        -Prompt "Select configuration [1-$($configurations.Count), Enter = keep $activeName, q = quit]" `
        -Count $configurations.Count
    if ($null -eq $choice) {
        Write-Host 'Cancelled; nothing changed.'
        return
    }

    $Configuration = if ($choice -eq 0) { $activeName } else { $configurations[$choice - 1].Name }
}

# --------------------------------------------------- resolve the selection ----
if ([string]::IsNullOrWhiteSpace($Configuration)) {
    $Configuration = $activeName
}
$configurationEntry = Get-HalStarterConfiguration -Configurations $configurations -Name $Configuration

if ($catalog.Presets.Count -eq 0) {
    throw "No APP_BUILD variation found in $($catalog.HeaderPath)"
}
$defaultPreset = Get-HalStarterDefaultPreset -Catalog $catalog

$currentPreset = Get-HalStarterSelectedPreset -RepoRoot $repoRoot -Configuration $Configuration
if ($currentPreset -and -not (Get-HalStarterPreset -Catalog $catalog -Name $currentPreset)) {
    $currentPreset = $null
}
if (-not $currentPreset) { $currentPreset = $defaultPreset }

if ($interactive) {
    Write-Host ''
    Write-Host "Configuration: $Configuration  ($($configurationEntry.Device))"
    Write-Host ''
    Write-Host 'APP_BUILD variations:'
    Write-PresetCatalog -Presets $catalog.Presets -SelectedName $currentPreset -DefaultName $defaultPreset
    Write-Host ''
    $choice = Read-MenuChoice `
        -Prompt "Select APP_BUILD [1-$($catalog.Presets.Count), Enter = keep $currentPreset, q = quit]" `
        -Count $catalog.Presets.Count
    if ($null -eq $choice) {
        Write-Host 'Cancelled; nothing changed.'
        return
    }

    $Preset = if ($choice -eq 0) { $currentPreset } else { $catalog.Presets[$choice - 1].Name }
}

if ([string]::IsNullOrWhiteSpace($Preset)) {
    # Scripted call without -Preset: keep this configuration's current choice.
    $Preset = $currentPreset
}

$presetEntry = Get-HalStarterPreset -Catalog $catalog -Name $Preset
if ($null -eq $presetEntry) {
    $allowed = ($catalog.Presets | ForEach-Object { $_.Name }) -join ', '
    throw "Unknown APP_BUILD variation '$Preset'. Available: $allowed"
}

# ------------------------------------------------------------------- apply ----
Set-HalStarterActiveConfiguration -ProjectDir $projectDir -ConfigurationEntry $configurationEntry
Set-HalStarterSelectedPreset -RepoRoot $repoRoot -Configuration $Configuration -Preset $Preset

Write-Host ''
Write-Host "Active configuration: $Configuration  ($($configurationEntry.Device))"
Write-Host "Active APP_BUILD:     $Preset"
if ($presetEntry.Detail) {
    Write-Host "                      $($presetEntry.Detail)"
}
Write-Host ''
Write-Host 'Next:'
Write-Host '  ./buildtools/build.ps1                          # follows the selection above'
Write-Host '  ./buildtools/flashauto.ps1 -Serial <PKOB4_SERIAL>'
