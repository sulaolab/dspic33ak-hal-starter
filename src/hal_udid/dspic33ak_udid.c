#include "dspic33ak_udid.h"

#include <stddef.h>
#include <stdint.h>

// UDID word addresses (unified address space; 4-byte aligned). See the header.
#define DSPIC33AK_UDID1_ADDRESS UINT32_C(0x007F2BE0)
#define DSPIC33AK_UDID2_ADDRESS UINT32_C(0x007F2BE4)
#define DSPIC33AK_UDID3_ADDRESS UINT32_C(0x007F2BE8)
#define DSPIC33AK_UDID4_ADDRESS UINT32_C(0x007F2BEC)

// Read one 32-bit word from an absolute address. A volatile const pointer is used
// so the compiler cannot elide or reorder the read. dsPIC33A is a unified address
// space, so no PSV / table-read setup is needed (unlike dsPIC33C/E/F).
static uint32_t DSPIC33AK_UDID_ReadWord(uint32_t address)
{
    const volatile uint32_t *source;

    source = (const volatile uint32_t *)(uintptr_t)address;
    return *source;
}

bool DSPIC33AK_UDID_Read(dspic33ak_udid_t *udid)
{
    if (udid == NULL)
    {
        return false;
    }

    udid->word[0] = DSPIC33AK_UDID_ReadWord(DSPIC33AK_UDID1_ADDRESS);
    udid->word[1] = DSPIC33AK_UDID_ReadWord(DSPIC33AK_UDID2_ADDRESS);
    udid->word[2] = DSPIC33AK_UDID_ReadWord(DSPIC33AK_UDID3_ADDRESS);
    udid->word[3] = DSPIC33AK_UDID_ReadWord(DSPIC33AK_UDID4_ADDRESS);

    return DSPIC33AK_UDID_IsPlausible(udid);
}

bool DSPIC33AK_UDID_IsPlausible(const dspic33ak_udid_t *udid)
{
    bool all_zero = true;
    bool all_one = true;
    uint32_t index;

    if (udid == NULL)
    {
        return false;
    }

    for (index = 0U; index < DSPIC33AK_UDID_WORD_COUNT; index++)
    {
        if (udid->word[index] != UINT32_C(0x00000000))
        {
            all_zero = false;
        }

        if (udid->word[index] != UINT32_C(0xFFFFFFFF))
        {
            all_one = false;
        }
    }

    return !(all_zero || all_one);
}
