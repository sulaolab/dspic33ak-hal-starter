# Vendored HAL Manifest

This starter vendors source snapshots from the public HAL repositories so the
project can be cloned and built without Git submodules. The table records the
upstream commit that each `src/hal_xxx/` folder was checked against.

| Local folder | Upstream repository | Upstream commit |
|---|---|---|
| `src/hal_gpio/` | `sulaolab/dspic33ak-gpio-hal` | `80ce7558ef9d0c9651ca3c132c8f060e7be9d8f1` |
| `src/hal_uart/` | `sulaolab/dspic33ak-uart-hal` | `a874b8dec481919f0d23ba9479dcd0d0900ad94e` |
| `src/hal_spi/` | `sulaolab/dspic33ak-spi-hal` | `44620edd88b1cf7957872eea9e5f50b782d39072` |
| `src/hal_i2c/` | `sulaolab/dspic33ak-i2c-hal` | `33dfee6371c0c61d15e1d318cb87f94358e993ac` |
| `src/hal_can/` | `sulaolab/dspic33ak-can-hal` | `c9a418a9cc1af5c63cf9c1234e8d300de7b1c618` |
| `src/hal_timer/` | `sulaolab/dspic33ak-timer-hal` | `880ebee58f5d61a631dbe7e704218fc69ba2af93` |

The source files in these folders were compared against the local upstream
repository checkouts at the commits above during the `tidy-hal-vendor-layout`
branch work.

`src/hal_udid/` is intentionally excluded from this table because it is currently
a local boot-banner helper rather than a standalone public HAL snapshot.
