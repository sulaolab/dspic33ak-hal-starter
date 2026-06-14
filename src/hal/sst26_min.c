/*
 * sst26_min.c
 * -----------
 * Minimal SST26 SPI-NOR flash driver: JEDEC ID read + a self-contained sector
 * erase/write/read-back verify. See sst26_min.h. Bus transfers go through the
 * SPI HAL (SPI4); CS/RST are driven via the GPIO HAL; pins/PPS are owned by the
 * board layer.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "dspic33ak_spi.h"
#include "dspic33ak_gpio.h"
#include "board_pins.h"
#include "board.h"
#include "sst26_min.h"

/* SPI4 config (matches the values validated on this board). */
#define SST26_SPI_INSTANCE   DSPIC33AK_SPI_INST_4
#define SST26_SPI_PBCLK_HZ   (100000000UL)
#define SST26_SPI_SCK_HZ     (12500000UL)
#define SST26_SPI_MODE       DSPIC33AK_SPI_MODE_0

/* SST26 command opcodes (subset). */
#define CMD_WREN     (0x06u)
#define CMD_RDSR     (0x05u)
#define CMD_WRSR     (0x01u)
#define CMD_RDCR     (0x35u)
#define CMD_SECT_ER  (0x20u)   /* 4 KB sector erase */
#define CMD_PAGE_PP  (0x02u)   /* page program      */
#define CMD_FASTREAD (0x0Bu)
#define CMD_JEDEC    (0x9Fu)

#define SR_WIP       (0x01u)   /* RDSR write-in-progress bit */

static dspic33ak_spi_handle_t s_spi;

#define CS_ASSERT()    (void)dspic33ak_gpio_clear(BOARD_SST26_PIN_CS)   /* active low */
#define CS_DEASSERT()  (void)dspic33ak_gpio_set(BOARD_SST26_PIN_CS)
#define XFER(b)        dspic33ak_spi_transfer8(&s_spi, (uint8_t)(b))

static void short_delay(void)
{
    for (volatile uint32_t i = 0u; i < 200000u; i++) { /* spin */ }
}

static uint8_t sst26_rdsr(void)
{
    CS_ASSERT();
    (void)XFER(CMD_RDSR);
    uint8_t sr = XFER(0x00);
    CS_DEASSERT();
    return sr;
}

static uint8_t sst26_rdcr(void)
{
    CS_ASSERT();
    (void)XFER(CMD_RDCR);
    uint8_t cr = XFER(0x00);
    CS_DEASSERT();
    return cr;
}

static void sst26_wait_wip_clear(void)
{
    while ((sst26_rdsr() & SR_WIP) != 0u) { /* poll */ }
}

static void sst26_write_enable(void)
{
    CS_ASSERT();
    (void)XFER(CMD_WREN);
    CS_DEASSERT();
}

/* Clear block-protection (BP=0) so the sector can be erased/programmed. */
static void sst26_unprotect_all(void)
{
    uint8_t cr = sst26_rdcr();   /* preserve config register */
    sst26_write_enable();
    CS_ASSERT();
    (void)XFER(CMD_WRSR);
    (void)XFER(0x00);            /* STATUS: BPL=0, BP=0 (all unprotected) */
    (void)XFER(cr);              /* CONFIG: unchanged                     */
    CS_DEASSERT();
    sst26_wait_wip_clear();
}

static void sst26_sector_erase_4k(uint32_t addr)
{
    sst26_write_enable();
    CS_ASSERT();
    (void)XFER(CMD_SECT_ER);
    (void)XFER((addr >> 16) & 0xFFu);
    (void)XFER((addr >>  8) & 0xFFu);
    (void)XFER((addr >>  0) & 0xFFu);
    CS_DEASSERT();
    sst26_wait_wip_clear();
}

static void sst26_page_program(uint32_t addr, const uint8_t *data, size_t len)
{
    sst26_write_enable();
    CS_ASSERT();
    (void)XFER(CMD_PAGE_PP);
    (void)XFER((addr >> 16) & 0xFFu);
    (void)XFER((addr >>  8) & 0xFFu);
    (void)XFER((addr >>  0) & 0xFFu);
    for (size_t i = 0u; i < len; i++) {
        (void)XFER(data[i]);
    }
    dspic33ak_spi_wait_done(&s_spi);   /* shifter idle before CS high */
    CS_DEASSERT();
    sst26_wait_wip_clear();
}

static void sst26_fast_read(uint32_t addr, uint8_t *buf, size_t len)
{
    CS_ASSERT();
    (void)XFER(CMD_FASTREAD);
    (void)XFER((addr >> 16) & 0xFFu);
    (void)XFER((addr >>  8) & 0xFFu);
    (void)XFER((addr >>  0) & 0xFFu);
    (void)XFER(0x00);                  /* dummy byte */
    for (size_t i = 0u; i < len; i++) {
        buf[i] = XFER(0x00);
    }
    CS_DEASSERT();
}

bool sst26_min_init(void)
{
    /* Board pins + PPS (RST left asserted low), then complete the reset pulse. */
    board_spi4_sst26_pins_init();
    CS_DEASSERT();
    short_delay();
    (void)dspic33ak_gpio_set(BOARD_SST26_PIN_RST);   /* release reset */
    short_delay();

    const dspic33ak_spi_config_t cfg = {
        .instance          = SST26_SPI_INSTANCE,
        .peripheralClockHz = SST26_SPI_PBCLK_HZ,
        .targetSckHz       = SST26_SPI_SCK_HZ,
        .mode              = SST26_SPI_MODE,
    };
    return dspic33ak_spi_init(&s_spi, &cfg);
}

bool sst26_min_read_jedec(uint8_t *mfr, uint8_t *type, uint8_t *dev)
{
    CS_ASSERT();
    (void)XFER(CMD_JEDEC);
    uint8_t m = XFER(0x00);
    uint8_t t = XFER(0x00);
    uint8_t d = XFER(0x00);
    CS_DEASSERT();

    if (mfr  != NULL) { *mfr  = m; }
    if (type != NULL) { *type = t; }
    if (dev  != NULL) { *dev  = d; }

    return (m == SST26_JEDEC_MFR) && (t == SST26_JEDEC_TYPE) && (d == SST26_JEDEC_DEV);
}

bool sst26_min_selftest(uint32_t addr)
{
    uint8_t tx[64];
    uint8_t rx[64];

    for (size_t i = 0u; i < sizeof(tx); i++) {
        tx[i] = (uint8_t)(0xA5u ^ (uint8_t)i);
    }

    sst26_unprotect_all();
    sst26_sector_erase_4k(addr);
    sst26_page_program(addr, tx, sizeof(tx));
    sst26_fast_read(addr, rx, sizeof(rx));

    for (size_t i = 0u; i < sizeof(tx); i++) {
        if (rx[i] != tx[i]) {
            return false;
        }
    }
    return true;
}
