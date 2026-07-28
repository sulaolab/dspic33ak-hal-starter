#ifndef APP_BUILD_CONFIG_H
#define APP_BUILD_CONFIG_H

/*
 * app_build_config.h
 * -------------------
 * Single source of truth for this starter's APP_BUILD variation catalog.
 *
 * APP_BUILD selects exactly one variation of the demo firmware. Each variation
 * below unconditionally defines every app-layer demo toggle that otherwise
 * defaults in app_config.h / can_bus_test.h, so a build is fully determined by
 * -DAPP_BUILD=<value> alone (no partial overrides). This header is included
 * first by app_config.h and can_bus_test.h; a plain MPLAB X IDE build with no
 * command-line defines at all still works -- APP_BUILD then defaults to
 * APP_BUILD_STARTER_DEFAULT below, which reproduces today's shipped defaults.
 *
 * Keep the numeric APP_BUILD values stable: buildtools/switch_config.ps1 reads
 * this file's #define lines (and the trailing "// detail" comment) to build its
 * menu, and a running build may already reference a value with -DAPP_BUILD=....
 *
 * One variation (APP_BUILD_TDM_NEG_TEST_2LEG) also needs the HAL-layer macro
 * DSPIC33AK_TDM_USE_SPI2=1. That is intentionally NOT set here: this header is
 * app-layer, and dspic33ak_spi_i2s_tdm_conf.h (the HAL's own config; see its
 * header comment) must never gain an app-layer #include -- the HAL MUST NOT
 * read app config. buildtools/build.ps1 passes that one macro on the compiler
 * command line for that preset only.
 */

/* Variation catalog. buildtools/switch_config.ps1 parses "NAME (n) // detail". */
#define APP_BUILD_STARTER_DEFAULT              (1) // TDM8 smoke demo (FS_50PCT); all self-tests off (shipped default)
#define APP_BUILD_TDM_SMOKE_OFF                (2) // TDM smoke demo off; frees MikroBUS-A SPI pins for a Click board
#define APP_BUILD_TDM_SMOKE_FS_PULSE           (3) // TDM smoke demo with FS_PULSE waveform instead of FS_50PCT
#define APP_BUILD_TDM_FS_RUNTIME_SWITCH_TEST   (4) // Opt-in FS-pin PPS restore self-test (requires FS_50PCT)
#define APP_BUILD_UART_ASYNC_SELFTEST          (5) // Opt-in UART1 async TX/RX self-test before the boot banner
#define APP_BUILD_TDM_NEG_TEST_1LEG            (6) // HAL negative-validation self-test, single-leg matrix; smoke resumes after
#define APP_BUILD_TDM_NEG_TEST_2LEG            (7) // HAL negative-validation self-test, 2-leg matrix (build.ps1 also injects DSPIC33AK_TDM_USE_SPI2=1); smoke off
#define APP_BUILD_CAN_BUS_TEST_ORIGINATOR      (8) // Two-board CAN FD bus test; this board is the ORIGINATOR (id 0x0A0)
#define APP_BUILD_CAN_BUS_TEST_ECHO            (9) // Two-board CAN FD bus test; this board is the ECHO (id 0x0B0)

#ifndef APP_BUILD
#define APP_BUILD (APP_BUILD_STARTER_DEFAULT)
#endif

#if (APP_BUILD == APP_BUILD_STARTER_DEFAULT)
  #define APP_BUILD_NAME    "APP_BUILD_STARTER_DEFAULT"
  #define APP_BUILD_DETAIL  "TDM8 smoke demo (FS_50PCT); all self-tests off"
  #define HAL_STARTER_ENABLE_TDM_SMOKE_DEMO       1
  #define APP_TDM_MASTER_FS50_BY_CLC10            1
  #define APP_TDM_FS_RUNTIME_SWITCH_TEST          0
  #define HAL_STARTER_ENABLE_UART_ASYNC_SELFTEST  0
  #define HAL_STARTER_ENABLE_TDM_NEG_TEST         0
  #define CAN_BUS_TEST                            0
  #define CAN_BUS_TEST_ECHO                       0
#elif (APP_BUILD == APP_BUILD_TDM_SMOKE_OFF)
  #define APP_BUILD_NAME    "APP_BUILD_TDM_SMOKE_OFF"
  #define APP_BUILD_DETAIL  "TDM smoke demo off; MikroBUS-A SPI pins free for a Click board"
  #define HAL_STARTER_ENABLE_TDM_SMOKE_DEMO       0
  #define APP_TDM_MASTER_FS50_BY_CLC10            1
  #define APP_TDM_FS_RUNTIME_SWITCH_TEST          0
  #define HAL_STARTER_ENABLE_UART_ASYNC_SELFTEST  0
  #define HAL_STARTER_ENABLE_TDM_NEG_TEST         0
  #define CAN_BUS_TEST                            0
  #define CAN_BUS_TEST_ECHO                       0
#elif (APP_BUILD == APP_BUILD_TDM_SMOKE_FS_PULSE)
  #define APP_BUILD_NAME    "APP_BUILD_TDM_SMOKE_FS_PULSE"
  #define APP_BUILD_DETAIL  "TDM smoke demo with FS_PULSE waveform instead of FS_50PCT"
  #define HAL_STARTER_ENABLE_TDM_SMOKE_DEMO       1
  #define APP_TDM_MASTER_FS50_BY_CLC10            0
  #define APP_TDM_FS_RUNTIME_SWITCH_TEST          0
  #define HAL_STARTER_ENABLE_UART_ASYNC_SELFTEST  0
  #define HAL_STARTER_ENABLE_TDM_NEG_TEST         0
  #define CAN_BUS_TEST                            0
  #define CAN_BUS_TEST_ECHO                       0
#elif (APP_BUILD == APP_BUILD_TDM_FS_RUNTIME_SWITCH_TEST)
  #define APP_BUILD_NAME    "APP_BUILD_TDM_FS_RUNTIME_SWITCH_TEST"
  #define APP_BUILD_DETAIL  "Opt-in FS-pin PPS restore self-test (requires FS_50PCT)"
  #define HAL_STARTER_ENABLE_TDM_SMOKE_DEMO       1
  #define APP_TDM_MASTER_FS50_BY_CLC10            1
  #define APP_TDM_FS_RUNTIME_SWITCH_TEST          1
  #define HAL_STARTER_ENABLE_UART_ASYNC_SELFTEST  0
  #define HAL_STARTER_ENABLE_TDM_NEG_TEST         0
  #define CAN_BUS_TEST                            0
  #define CAN_BUS_TEST_ECHO                       0
#elif (APP_BUILD == APP_BUILD_UART_ASYNC_SELFTEST)
  #define APP_BUILD_NAME    "APP_BUILD_UART_ASYNC_SELFTEST"
  #define APP_BUILD_DETAIL  "Opt-in UART1 async TX/RX self-test before the boot banner"
  #define HAL_STARTER_ENABLE_TDM_SMOKE_DEMO       1
  #define APP_TDM_MASTER_FS50_BY_CLC10            1
  #define APP_TDM_FS_RUNTIME_SWITCH_TEST          0
  #define HAL_STARTER_ENABLE_UART_ASYNC_SELFTEST  1
  #define HAL_STARTER_ENABLE_TDM_NEG_TEST         0
  #define CAN_BUS_TEST                            0
  #define CAN_BUS_TEST_ECHO                       0
#elif (APP_BUILD == APP_BUILD_TDM_NEG_TEST_1LEG)
  #define APP_BUILD_NAME    "APP_BUILD_TDM_NEG_TEST_1LEG"
  #define APP_BUILD_DETAIL  "HAL negative-validation self-test, single-leg matrix; smoke resumes after"
  #define HAL_STARTER_ENABLE_TDM_SMOKE_DEMO       1
  #define APP_TDM_MASTER_FS50_BY_CLC10            1
  #define APP_TDM_FS_RUNTIME_SWITCH_TEST          0
  #define HAL_STARTER_ENABLE_UART_ASYNC_SELFTEST  0
  #define HAL_STARTER_ENABLE_TDM_NEG_TEST         1
  #define CAN_BUS_TEST                            0
  #define CAN_BUS_TEST_ECHO                       0
#elif (APP_BUILD == APP_BUILD_TDM_NEG_TEST_2LEG)
  #define APP_BUILD_NAME    "APP_BUILD_TDM_NEG_TEST_2LEG"
  #define APP_BUILD_DETAIL  "HAL negative-validation self-test, 2-leg matrix (needs DSPIC33AK_TDM_USE_SPI2=1); smoke off"
  #define HAL_STARTER_ENABLE_TDM_SMOKE_DEMO       0
  #define APP_TDM_MASTER_FS50_BY_CLC10            1
  #define APP_TDM_FS_RUNTIME_SWITCH_TEST          0
  #define HAL_STARTER_ENABLE_UART_ASYNC_SELFTEST  0
  #define HAL_STARTER_ENABLE_TDM_NEG_TEST         1
  #define CAN_BUS_TEST                            0
  #define CAN_BUS_TEST_ECHO                       0
#elif (APP_BUILD == APP_BUILD_CAN_BUS_TEST_ORIGINATOR)
  #define APP_BUILD_NAME    "APP_BUILD_CAN_BUS_TEST_ORIGINATOR"
  #define APP_BUILD_DETAIL  "Two-board CAN FD bus test; this board is the ORIGINATOR (id 0x0A0)"
  #define HAL_STARTER_ENABLE_TDM_SMOKE_DEMO       1
  #define APP_TDM_MASTER_FS50_BY_CLC10            1
  #define APP_TDM_FS_RUNTIME_SWITCH_TEST          0
  #define HAL_STARTER_ENABLE_UART_ASYNC_SELFTEST  0
  #define HAL_STARTER_ENABLE_TDM_NEG_TEST         0
  #define CAN_BUS_TEST                            1
  #define CAN_BUS_TEST_ECHO                       0
#elif (APP_BUILD == APP_BUILD_CAN_BUS_TEST_ECHO)
  #define APP_BUILD_NAME    "APP_BUILD_CAN_BUS_TEST_ECHO"
  #define APP_BUILD_DETAIL  "Two-board CAN FD bus test; this board is the ECHO (id 0x0B0)"
  #define HAL_STARTER_ENABLE_TDM_SMOKE_DEMO       1
  #define APP_TDM_MASTER_FS50_BY_CLC10            1
  #define APP_TDM_FS_RUNTIME_SWITCH_TEST          0
  #define HAL_STARTER_ENABLE_UART_ASYNC_SELFTEST  0
  #define HAL_STARTER_ENABLE_TDM_NEG_TEST         0
  #define CAN_BUS_TEST                            1
  #define CAN_BUS_TEST_ECHO                       1
#else
  #error "APP_BUILD is not a known variation. See app_build_config.h."
#endif

#endif /* APP_BUILD_CONFIG_H */
