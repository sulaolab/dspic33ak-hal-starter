#ifndef DSPIC33AK_I3C_REG_H
#define DSPIC33AK_I3C_REG_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Internal register helper layer.
 *
 * Keep XC-DSC / DFP register names isolated in dspic33ak_i3c_device.c.  Driver
 * logic should use this pointer table plus masks instead of public DFP bitfield
 * structures.
 */

typedef struct {
    volatile uint32_t *CON;
    volatile uint32_t *PG;
    volatile uint32_t *CNTRL;
    volatile uint32_t *ADD;
    volatile uint32_t *HWCAP;
    volatile uint32_t *CMDQUE;
    volatile uint32_t *RESPQUE;
    volatile uint32_t *TXRXDATA;
    volatile uint32_t *RSTCON;
    volatile uint32_t *INTSTA;
    volatile uint32_t *INTSTACON;
    volatile uint32_t *INTCON;
    volatile uint32_t *QLEVEL;
    volatile uint32_t *BUFLEVEL;
    volatile uint32_t *STATE;
    volatile uint32_t *I2CFMTIM;
    volatile uint32_t *I2CFMPTIM;
    volatile uint32_t *BUSTIM;
    volatile uint32_t *BUSIDLETIM;
    volatile uint32_t *CNTRLTIMOUT;
} dspic33ak_i3c_regs_t;

/* I3C1CON bits, common to the current dsPIC33AK-MP DFP naming. */
#define DSPIC33AK_I3C_CON_I2C      (1UL << 8)
#define DSPIC33AK_I3C_CON_SIDL     (1UL << 13)
#define DSPIC33AK_I3C_CON_ON       (1UL << 15)
#define DSPIC33AK_I3C_CON_EXTPUR   (1UL << 24)
#define DSPIC33AK_I3C_CON_INTPUR   (1UL << 25)
#define DSPIC33AK_I3C_CON_ALTI3C   (1UL << 26)

/* I3C1CNTRL bits. */
#define DSPIC33AK_I3C_CNTRL_I2CTGT  (1UL << 7)
#define DSPIC33AK_I3C_CNTRL_ABORT   (1UL << 29)
#define DSPIC33AK_I3C_CNTRL_RESUME  (1UL << 30)
#define DSPIC33AK_I3C_CNTRL_ENABLE  (1UL << 31)

/* I3C1RSTCON bits. */
#define DSPIC33AK_I3C_RSTCON_SOFTRST    (1UL << 0)
#define DSPIC33AK_I3C_RSTCON_CMDQRST    (1UL << 1)
#define DSPIC33AK_I3C_RSTCON_RESPQRST   (1UL << 2)
#define DSPIC33AK_I3C_RSTCON_TXFIFORST  (1UL << 3)
#define DSPIC33AK_I3C_RSTCON_RXFIFORST  (1UL << 4)
#define DSPIC33AK_I3C_RSTCON_IBIQRST    (1UL << 5)

/* I3C1INTSTA / INTCON useful first-pass masks. */
#define DSPIC33AK_I3C_INT_TXTHLD      (1UL << 0)
#define DSPIC33AK_I3C_INT_RXTHLD      (1UL << 1)
#define DSPIC33AK_I3C_INT_CMDQ        (1UL << 3)
#define DSPIC33AK_I3C_INT_RESPQ       (1UL << 4)
#define DSPIC33AK_I3C_INT_TRANSABT    (1UL << 5)
#define DSPIC33AK_I3C_INT_TRANSERR    (1UL << 9)
#define DSPIC33AK_I3C_INT_BUSRST      (1UL << 15)
#define DSPIC33AK_I3C_INT_START       (1UL << 16)
#define DSPIC33AK_I3C_INT_ALL_KNOWN   (DSPIC33AK_I3C_INT_TXTHLD | \
                                       DSPIC33AK_I3C_INT_RXTHLD | \
                                       DSPIC33AK_I3C_INT_CMDQ | \
                                       DSPIC33AK_I3C_INT_RESPQ | \
                                       DSPIC33AK_I3C_INT_TRANSABT | \
                                       DSPIC33AK_I3C_INT_TRANSERR | \
                                       DSPIC33AK_I3C_INT_BUSRST | \
                                       DSPIC33AK_I3C_INT_START)

static inline void dspic33ak_i3c_reg_set(volatile uint32_t *reg, uint32_t mask)
{
    *reg |= mask;
}

static inline void dspic33ak_i3c_reg_clear(volatile uint32_t *reg, uint32_t mask)
{
    *reg &= ~mask;
}

static inline void dspic33ak_i3c_reg_write(volatile uint32_t *reg, uint32_t value)
{
    *reg = value;
}

static inline bool dspic33ak_i3c_reg_is_set(volatile uint32_t *reg, uint32_t mask)
{
    return ((*reg & mask) != 0u);
}

#endif /* DSPIC33AK_I3C_REG_H */
