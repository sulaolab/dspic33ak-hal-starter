# resetauto.ps1 - reset-only shortcut. Equivalent to:  .\buildtools\flashauto.ps1 -Reset
#
# Just type this to reset the connected board (serial + device auto-detected) and
# see the compact [reset] heartbeat log. All flashauto.ps1 parameters pass through,
# e.g.:
#   .\buildtools\resetauto.ps1                       # auto-detect the single board
#   .\buildtools\resetauto.ps1 -Serial 020085204RYN000057
#   .\buildtools\resetauto.ps1 -Verbose              # raw IPECMDBoost output instead
#   .\buildtools\resetauto.ps1 -CleanJava            # clear stale boost state first
& (Join-Path $PSScriptRoot 'flashauto.ps1') -Reset @args
