/*
 * i2c_scan.c
 * ----------
 * I2C bus scanner sample. See i2c_scan.h. Uses only the master I2C HAL: a
 * 0-byte write addresses the device and issues STOP, returning OK if the address
 * was acknowledged or ERR_NACK if nothing responded.
 */

#include <stdio.h>
#include <stdint.h>

#include "dspic33ak_i2c_master.h"
#include "i2c_scan.h"

void i2c_scan_run(dspic33ak_i2c_instance_t inst)
{
    printf(" I2C scan (I2C%u): probing 0x08..0x77 ...\n", (unsigned)inst + 1u);

    int found = 0;
    for (uint8_t addr = 0x08u; addr <= 0x77u; addr++)
    {
        dspic33ak_i2c_status_t st = dspic33ak_i2c_write(inst, addr, NULL, 0u);
        if (st == DSPIC33AK_I2C_OK)
        {
            printf("   device ACK at 0x%02X\n", addr);
            found++;
        }
    }

    if (found == 0)
    {
        printf("   no devices found\n");
    }
    else
    {
        printf("   %d device(s) found\n", found);
    }
}
