#ifndef SST26_MIN_H
#define SST26_MIN_H

/*
 * sst26_min.h
 * -----------
 * Minimal SST26 (SPI NOR flash) driver for the starter: enough to read the
 * JEDEC ID and run a self-contained sector erase/write/read-back verify.
 *
 * The SPI bus transfer is provided by the SPI HAL (dspic33ak_spi); this driver
 * only issues flash commands and drives CS/RST. Pin routing (PPS + GPIO) is done
 * by the board layer (board_spi4_sst26_pins_init).
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Expected SST26 JEDEC ID bytes. */
#define SST26_JEDEC_MFR   (0xBFu)
#define SST26_JEDEC_TYPE  (0x26u)
#define SST26_JEDEC_DEV   (0x12u)

/* Bring up the SST26: board pins + reset pulse, then init the SPI HAL on SPI4.
 * Returns false if SPI4 is unavailable on the target or the SPI HAL failed to initialize. */
bool sst26_min_init(void);

/* Read the JEDEC ID (0x9F). Stores the three ID bytes (any pointer may be NULL)
 * and returns true if they match the expected SST26 values. */
bool sst26_min_read_jedec(uint8_t *mfr, uint8_t *type, uint8_t *dev);

/* Self-test on one 4 KB sector at `addr`: unprotect, erase, program a known
 * pattern, then read it back and compare. Returns true if the read-back matches.
 * Destroys the contents of that sector. */
bool sst26_min_selftest(uint32_t addr);

#ifdef __cplusplus
}
#endif

#endif /* SST26_MIN_H */
