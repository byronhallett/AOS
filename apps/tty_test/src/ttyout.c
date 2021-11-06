/*
 * Copyright 2019, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */
/****************************************************************************
 *
 *      $Id:  $
 *
 *      Description: Simple milestone 0 code.
 *                   Libc will need sos_write & sos_read implemented.
 *
 *      Author:      Ben Leslie
 *
 ****************************************************************************/

#include <stdarg.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "ttyout.h"

#include <sel4/sel4.h>

/*
SERIAL READ/WRITE PROTOCOL

TODO: move to external file for reference by main.c
*/
#define SOS_SYSCALL_SERIAL 2
#define SOS_SERIAL_CHAR_COUNT 1
#define SOS_SERIAL_DATA_START 2

void ttyout_init(void)
{
    /* Perform any initialisation you require here */
}

static size_t sos_debug_print(const void *vData, size_t count)
{
#ifdef CONFIG_DEBUG_BUILD
    size_t i;
    const char *realdata = vData;
    for (i = 0; i < count; i++) {
        seL4_DebugPutChar(realdata[i]);
    }
#endif
    return count;
}

size_t sos_write(void *vData, size_t count)
{
    if (count == 0) return 0;
    // treat data as words
    seL4_Word* wData = (seL4_Word*) vData;
    // our call ID
    seL4_SetMR(0, SOS_SYSCALL_SERIAL);
    // how many 64 bit words needed?
    int word_count = count / 8 + 1;

    // first word contains number of characters to print
    seL4_SetMR(SOS_SERIAL_CHAR_COUNT, count);

    // loop over data to set the words
    for (size_t i = SOS_SERIAL_DATA_START; i <= word_count+SOS_SERIAL_DATA_START; i++)
    {
      seL4_SetMR(i, wData[i-SOS_SERIAL_DATA_START]); // data is passed as uint64
    }

    // info about our call
    // length is word_count + 1 for char count
    seL4_MessageInfo_t info = seL4_MessageInfo_new(0, 0, 0, word_count+1);
    seL4_MessageInfo_t reply_info = seL4_Call(SYSCALL_ENDPOINT_SLOT, info);
    // MR0 contains the number of characters written
    return seL4_GetMR(0);
}

size_t sos_read(void *vData, size_t count)
{
    //implement this to use your syscall
    return 0;
}

