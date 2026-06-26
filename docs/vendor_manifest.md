# Vendored HAL Manifest

This starter vendors source snapshots from the public HAL repositories so the
project can be cloned and built without Git submodules. The table records the
upstream commit that each `src/hal_xxx/` folder was checked against.

| Local folder | Upstream repository | Upstream commit |
|---|---|---|
| `src/hal_gpio/` | `sulaolab/dspic33ak-gpio-hal` | `582e18f` (branch `sync-review-fixes`; pending merge to main — see note below) |
| `src/hal_uart/` | `sulaolab/dspic33ak-uart-hal` | `a874b8dec481919f0d23ba9479dcd0d0900ad94e` |
| `src/hal_spi/` | `sulaolab/dspic33ak-spi-hal` | `44620edd88b1cf7957872eea9e5f50b782d39072` |
| `src/hal_i2c/` | `sulaolab/dspic33ak-i2c-hal` | `33dfee6371c0c61d15e1d318cb87f94358e993ac` |
| `src/hal_can/` | `sulaolab/dspic33ak-can-hal` | `c9a418a9cc1af5c63cf9c1234e8d300de7b1c618` |
| `src/hal_timer/` | `sulaolab/dspic33ak-timer-hal` | `bc739d3c85fc8b2d0d8c608c3392d2b5b404c6c7` |

Unless noted below, the source files in these folders were compared against the
local upstream repository checkouts at the commits above during the
`tidy-hal-vendor-layout` branch work.

**Note on `src/hal_gpio/`:** the current snapshot includes the reviewed, hardware-
validated all-in GPIO+PPS HAL (RP-first API, PPS companion layer, full attribute
setters) developed in the Perseus project. This version is on the
`sync-review-fixes` branch of `sulaolab/dspic33ak-gpio-hal` and has not yet been
merged to that repository's `main`. Once merged, update the SHA above to the final
`main` commit. The `src/hal_gpio/dspic33ak_gpio_event.*` CN event files are
starter-developed and are not covered by the `dspic33ak-gpio-hal` snapshot.

`src/hal_udid/` is intentionally excluded from this table because it is currently
a local boot-banner helper rather than a standalone public HAL snapshot.
