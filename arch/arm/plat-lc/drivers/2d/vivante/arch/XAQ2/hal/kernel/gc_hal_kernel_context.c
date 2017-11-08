/****************************************************************************
*
*    Copyright (C) 2005 - 2013 by Vivante Corp.
*
*    This program is free software; you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation; either version 2 of the license, or
*    (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program; if not write to the Free Software
*    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*
*****************************************************************************/


#include "gc_hal.h"
#include "gc_hal_kernel.h"
#include "gc_hal_kernel_context.h"
#include "gc_hal_kernel_buffer.h"

/******************************************************************************\
******************************** Debugging Macro *******************************
\******************************************************************************/

/* Zone used for header/footer. */
#define _GC_OBJ_ZONE    gcvZONE_HARDWARE


/******************************************************************************\
************************** Context State Buffer Helpers ************************
\******************************************************************************/

#define _STATE(reg)                                                            \
    _State(\
        Context, index, \
        reg ## _Address >> 2, \
        reg ## _ResetValue, \
        reg ## _Count, \
        gcvFALSE, gcvFALSE                                                     \
        )

#define _STATE_COUNT(reg, count)                                               \
    _State(\
        Context, index, \
        reg ## _Address >> 2, \
        reg ## _ResetValue, \
        count, \
        gcvFALSE, gcvFALSE                                                     \
        )

#define _STATE_COUNT_OFFSET(reg, offset, count)                                \
    _State(\
        Context, index, \
        (reg ## _Address >> 2) + offset, \
        reg ## _ResetValue, \
        count, \
        gcvFALSE, gcvFALSE                                                     \
        )

#define _STATE_MIRROR_COUNT(reg, mirror, count)                                \
    _StateMirror(\
        Context, \
        reg ## _Address >> 2, \
        count, \
        mirror ## _Address >> 2                                                \
        )

#define _STATE_HINT(reg)                                                       \
    _State(\
        Context, index, \
        reg ## _Address >> 2, \
        reg ## _ResetValue, \
        reg ## _Count, \
        gcvFALSE, gcvTRUE                                                      \
        )

#define _STATE_HINT_BLOCK(reg, block, count)                                   \
    _State(\
        Context, index, \
        (reg ## _Address >> 2) + (block << reg ## _BLK), \
        reg ## _ResetValue, \
        count, \
        gcvFALSE, gcvTRUE                                                      \
        )

#define _STATE_X(reg)                                                          \
    _State(\
        Context, index, \
        reg ## _Address >> 2, \
        reg ## _ResetValue, \
        reg ## _Count, \
        gcvTRUE, gcvFALSE                                                      \
        )

#define _CLOSE_RANGE()                                                         \
    _TerminateStateBlock(Context, index)

#define _ENABLE(reg, field)                                                    \
    do                                                                         \
    {                                                                          \
        if (gcmVERIFYFIELDVALUE(data, reg, MASK_ ## field, ENABLED))           \
        {                                                                      \
            enable |= gcmFIELDMASK(reg, field);                                \
        }                                                                      \
    }                                                                          \
    while (gcvFALSE)

#define _BLOCK_COUNT(reg)                                                      \
    ((reg ## _Count) >> (reg ## _BLK))


/******************************************************************************\
*********************** Support Functions and Definitions **********************
\******************************************************************************/

#define gcdSTATE_MASK \
    (((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x03 | 0xC0FFEE & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))))



static gctSIZE_T
_FlushPipe(
    IN gckCONTEXT Context,
    IN gctSIZE_T Index,
    IN gcePIPE_SELECT Pipe
    )
{
    if (Context->buffer != gcvNULL)
    {
        gctUINT32_PTR buffer;

        /* Address correct index. */
        buffer = Context->buffer->logical + Index;

        /* Flush the current pipe. */
        *buffer++
            = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E03) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

        *buffer++
            = (Pipe == gcvPIPE_2D)
                ?   ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3)))
                :   ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))
                  | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1)))
                  | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 2:2) - (0 ? 2:2) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:2) - (0 ? 2:2) + 1))))))) << (0 ? 2:2))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 2:2) - (0 ? 2:2) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:2) - (0 ? 2:2) + 1))))))) << (0 ? 2:2)))
                  | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4)));

        /* Semaphore from FE to PE. */
        *buffer++
            = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E02) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

        *buffer++
            = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)));

        /* Stall from FE to PE. */
        *buffer++
            = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x09 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)));

        *buffer
            = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)));
    }

    /* Flushing 3D pipe takes 6 slots. */
    return 6;
}


static gctSIZE_T
_SwitchPipe(
    IN gckCONTEXT Context,
    IN gctSIZE_T Index,
    IN gcePIPE_SELECT Pipe
    )
{
    if (Context->buffer != gcvNULL)
    {
        gctUINT32_PTR buffer;

        /* Address correct index. */
        buffer = Context->buffer->logical + Index;

        /* LoadState(AQPipeSelect, 1), pipe. */
        *buffer++
            = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E00) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)));

        *buffer
            = (Pipe == gcvPIPE_2D)
                ? 0x1
                : 0x0;
    }

    return 2;
}


static gceSTATUS
_InitializeContextBuffer(
    IN gckCONTEXT Context
    )
{
    gctUINT32_PTR buffer;
    gctSIZE_T index;


    /* Reset the buffer index. */
    index = 0;

    /* Reset the last state address. */
    Context->lastAddress = ~0U;

    /* Get the buffer pointer. */
    buffer = (Context->buffer == gcvNULL)
        ? gcvNULL
        : Context->buffer->logical;


    /**************************************************************************/
    /* Build 2D states. *******************************************************/



    /**************************************************************************/
    /* Link to another address. ***********************************************/

    Context->linkIndex3D = index;

    if (buffer != gcvNULL)
    {
        buffer[index + 0]
            = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x08 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

        buffer[index + 1]
            = 0;
    }

    index += 2;

    /* Store the end of the context buffer. */
    Context->bufferSize = index * gcmSIZEOF(gctUINT32);


    /**************************************************************************/
    /* Pipe switch for the case where neither 2D nor 3D are used. *************/

    /* Store the 3D entry index. */
    Context->entryOffsetXDFrom2D = index * gcmSIZEOF(gctUINT32);

    /* Flush 2D pipe. */
    index += _FlushPipe(Context, index, gcvPIPE_2D);

    /* Switch to 3D pipe. */
    index += _SwitchPipe(Context, index, gcvPIPE_3D);

    /* Store the location of the link. */
    Context->linkIndexXD = index;

    if (buffer != gcvNULL)
    {
        buffer[index + 0]
            = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x08 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

        buffer[index + 1]
            = 0;
    }

    index += 2;


    /**************************************************************************/
    /* Save size for buffer. **************************************************/

    Context->totalSize = index * gcmSIZEOF(gctUINT32);


    /* Success. */
    return gcvSTATUS_OK;
}

static gceSTATUS
_DestroyContext(
    IN gckCONTEXT Context
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    if (Context != gcvNULL)
    {
        gcsCONTEXT_PTR bufferHead;

        /* Free context buffers. */
        for (bufferHead = Context->buffer; Context->buffer != gcvNULL;)
        {
            /* Get a shortcut to the current buffer. */
            gcsCONTEXT_PTR buffer = Context->buffer;

            /* Get the next buffer. */
            gcsCONTEXT_PTR next = buffer->next;

            /* Last item? */
            if (next == bufferHead)
            {
                next = gcvNULL;
            }

            /* Destroy the signal. */
            if (buffer->signal != gcvNULL)
            {
                gcmkONERROR(gckOS_DestroySignal(
                    Context->os, buffer->signal
                    ));

                buffer->signal = gcvNULL;
            }

            /* Free state delta map. */
            if (buffer->logical != gcvNULL)
            {
#if gcdVIRTUAL_COMMAND_BUFFER
                gcmkONERROR(gckEVENT_DestroyVirtualCommandBuffer(
                    Context->hardware->kernel->eventObj,
                    Context->totalSize,
                    buffer->physical,
                    buffer->logical,
                    gcvKERNEL_PIXEL
                    ));

#else
                gcmkONERROR(gckEVENT_FreeContiguousMemory(
                    Context->hardware->kernel->eventObj,
                    Context->totalSize,
                    buffer->physical,
                    buffer->logical,
                    gcvKERNEL_PIXEL
                    ));
#endif

                buffer->logical = gcvNULL;
            }

            /* Free context buffer. */
            gcmkONERROR(gcmkOS_SAFE_FREE(Context->os, buffer));

            /* Remove from the list. */
            Context->buffer = next;
        }

#if gcdSECURE_USER
        /* Free the hint array. */
        if (Context->hint != gcvNULL)
        {
            gcmkONERROR(gcmkOS_SAFE_FREE(Context->os, Context->hint));
        }
#endif
        /* Free record array copy. */
        if (Context->recordArray != gcvNULL)
        {
            gcmkONERROR(gcmkOS_SAFE_FREE(Context->os, Context->recordArray));
        }

        /* Free the state mapping. */
        if (Context->map != gcvNULL)
        {
            gcmkONERROR(gcmkOS_SAFE_FREE(Context->os, Context->map));
        }

        /* Mark the gckCONTEXT object as unknown. */
        Context->object.type = gcvOBJ_UNKNOWN;

        /* Free the gckCONTEXT object. */
        gcmkONERROR(gcmkOS_SAFE_FREE(Context->os, Context));
    }

OnError:
    return status;
}


/******************************************************************************\
**************************** Context Management API ****************************
\******************************************************************************/

/******************************************************************************\
**
**  gckCONTEXT_Construct
**
**  Construct a new gckCONTEXT object.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to gckOS object.
**
**      gctUINT32 ProcessID
**          Current process ID.
**
**      gckHARDWARE Hardware
**          Pointer to gckHARDWARE object.
**
**  OUTPUT:
**
**      gckCONTEXT * Context
**          Pointer to a variable thet will receive the gckCONTEXT object
**          pointer.
*/
gceSTATUS
gckCONTEXT_Construct(
    IN gckOS Os,
    IN gckHARDWARE Hardware,
    IN gctUINT32 ProcessID,
    OUT gckCONTEXT * Context
    )
{
    gceSTATUS status;
    gckCONTEXT context = gcvNULL;
    gctSIZE_T allocationSize;
    gctUINT i;
    gctPOINTER pointer = gcvNULL;

    gcmkHEADER_ARG("Os=0x%08X Hardware=0x%08X", Os, Hardware);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Context != gcvNULL);


    /**************************************************************************/
    /* Allocate and initialize basic fields of gckCONTEXT. ********************/

    /* The context object size. */
    allocationSize = gcmSIZEOF(struct _gckCONTEXT);

    /* Allocate the object. */
    gcmkONERROR(gckOS_Allocate(
        Os, allocationSize, &pointer
        ));

    context = pointer;

    /* Reset the entire object. */
    gcmkONERROR(gckOS_ZeroMemory(context, allocationSize));

    /* Initialize the gckCONTEXT object. */
    context->object.type = gcvOBJ_CONTEXT;
    context->os          = Os;
    context->hardware    = Hardware;


    context->entryPipe = gcvPIPE_2D;
    context->exitPipe  = gcvPIPE_2D;

    /* Get the command buffer requirements. */
    gcmkONERROR(gckHARDWARE_QueryCommandBuffer(
        Hardware,
        &context->alignment,
        &context->reservedHead,
        &context->reservedTail
        ));

    /* Mark the context as dirty to force loading of the entire state table
       the first time. */
    context->dirty = gcvTRUE;


    /**************************************************************************/
    /* Get the size of the context buffer. ************************************/

    gcmkONERROR(_InitializeContextBuffer(context));


    /**************************************************************************/
    /* Compute the size of the record array. **********************************/

    context->recordArraySize
        = gcmSIZEOF(gcsSTATE_DELTA_RECORD) * context->stateCount;


    if (context->stateCount > 0)
    {
        /**************************************************************************/
        /* Allocate and reset the state mapping table. ****************************/

        /* Allocate the state mapping table. */
        gcmkONERROR(gckOS_Allocate(
            Os,
            gcmSIZEOF(gcsSTATE_MAP) * context->stateCount,
            &pointer
            ));

        context->map = pointer;

        /* Zero the state mapping table. */
        gcmkONERROR(gckOS_ZeroMemory(
            context->map, gcmSIZEOF(gcsSTATE_MAP) * context->stateCount
            ));


        /**************************************************************************/
        /* Allocate the hint array. ***********************************************/

#if gcdSECURE_USER
        /* Allocate hints. */
        gcmkONERROR(gckOS_Allocate(
            Os,
            gcmSIZEOF(gctBOOL) * context->stateCount,
            &pointer
            ));

        context->hint = pointer;
#endif
    }

    /**************************************************************************/
    /* Allocate the context and state delta buffers. **************************/

    for (i = 0; i < gcdCONTEXT_BUFFER_COUNT; i += 1)
    {
        /* Allocate a context buffer. */
        gcsCONTEXT_PTR buffer;

        /* Allocate the context buffer structure. */
        gcmkONERROR(gckOS_Allocate(
            Os,
            gcmSIZEOF(gcsCONTEXT),
            &pointer
            ));

        buffer = pointer;

        /* Reset the context buffer structure. */
        gcmkVERIFY_OK(gckOS_ZeroMemory(
            buffer, gcmSIZEOF(gcsCONTEXT)
            ));

        /* Append to the list. */
        if (context->buffer == gcvNULL)
        {
            buffer->next    = buffer;
            context->buffer = buffer;
        }
        else
        {
            buffer->next          = context->buffer->next;
            context->buffer->next = buffer;
        }

        /* Set the number of delta in the order of creation. */
#if gcmIS_DEBUG(gcdDEBUG_CODE)
        buffer->num = i;
#endif

        /* Create the busy signal. */
        gcmkONERROR(gckOS_CreateSignal(
            Os, gcvFALSE, &buffer->signal
            ));

        /* Set the signal, buffer is currently not busy. */
        gcmkONERROR(gckOS_Signal(
            Os, buffer->signal, gcvTRUE
            ));

        /* Create a new physical context buffer. */
#if gcdVIRTUAL_COMMAND_BUFFER
        gcmkONERROR(gckKERNEL_AllocateVirtualCommandBuffer(
            context->hardware->kernel,
            gcvFALSE,
            &context->totalSize,
            &buffer->physical,
            &pointer
            ));

#else
        gcmkONERROR(gckOS_AllocateContiguous(
            Os,
            gcvFALSE,
            &context->totalSize,
            &buffer->physical,
            &pointer
            ));
#endif

        buffer->logical = pointer;

        /* Set gckEVENT object pointer. */
        buffer->eventObj = Hardware->kernel->eventObj;

        /* Set the pointers to the LINK commands. */
        if (context->linkIndex2D != 0)
        {
            buffer->link2D = &buffer->logical[context->linkIndex2D];
        }

        if (context->linkIndex3D != 0)
        {
            buffer->link3D = &buffer->logical[context->linkIndex3D];
        }

        if (context->linkIndexXD != 0)
        {
            gctPOINTER xdLink;
            gctUINT8_PTR xdEntryLogical;
            gctSIZE_T xdEntrySize;
            gctSIZE_T linkBytes;

            /* Determine LINK parameters. */
            xdLink
                = &buffer->logical[context->linkIndexXD];

            xdEntryLogical
                = (gctUINT8_PTR) buffer->logical
                + context->entryOffsetXDFrom3D;

            xdEntrySize
                = context->bufferSize
                - context->entryOffsetXDFrom3D;

            /* Query LINK size. */
            gcmkONERROR(gckHARDWARE_Link(
                Hardware, gcvNULL, gcvNULL, 0, &linkBytes
                ));

            /* Generate a LINK. */
            gcmkONERROR(gckHARDWARE_Link(
                Hardware,
                xdLink,
                xdEntryLogical,
                xdEntrySize,
                &linkBytes
                ));
        }
    }


    /**************************************************************************/
    /* Initialize the context buffers. ****************************************/

    /* Initialize the current context buffer. */
    gcmkONERROR(_InitializeContextBuffer(context));

    /* Make all created contexts equal. */
    {
        gcsCONTEXT_PTR currContext, tempContext;

        /* Set the current context buffer. */
        currContext = context->buffer;

        /* Get the next context buffer. */
        tempContext = currContext->next;

        /* Loop through all buffers. */
        while (tempContext != currContext)
        {
            if (tempContext == gcvNULL)
            {
                gcmkONERROR(gcvSTATUS_NOT_FOUND);
            }

            /* Copy the current context. */
            gckOS_MemCopy(
                tempContext->logical,
                currContext->logical,
                context->totalSize
                );

            /* Get the next context buffer. */
            tempContext = tempContext->next;
        }
    }

    /* Return pointer to the gckCONTEXT object. */
    *Context = context;

    /* Success. */
    gcmkFOOTER_ARG("*Context=0x%08X", *Context);
    return gcvSTATUS_OK;

OnError:
    /* Roll back on error. */
    gcmkVERIFY_OK(_DestroyContext(context));

    /* Return the status. */
    gcmkFOOTER();
    return status;
}

/******************************************************************************\
**
**  gckCONTEXT_Destroy
**
**  Destroy a gckCONTEXT object.
**
**  INPUT:
**
**      gckCONTEXT Context
**          Pointer to an gckCONTEXT object.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckCONTEXT_Destroy(
    IN gckCONTEXT Context
    )
{
    gceSTATUS status;

    gcmkHEADER_ARG("Context=0x%08X", Context);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Context, gcvOBJ_CONTEXT);

    /* Destroy the context and all related objects. */
    status = _DestroyContext(Context);

    /* Success. */
    gcmkFOOTER_NO();
    return status;
}

/******************************************************************************\
**
**  gckCONTEXT_Update
**
**  Merge all pending state delta buffers into the current context buffer.
**
**  INPUT:
**
**      gckCONTEXT Context
**          Pointer to an gckCONTEXT object.
**
**      gctUINT32 ProcessID
**          Current process ID.
**
**      gcsSTATE_DELTA_PTR StateDelta
**          Pointer to the state delta.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckCONTEXT_Update(
    IN gckCONTEXT Context,
    IN gctUINT32 ProcessID,
    IN gcsSTATE_DELTA_PTR StateDelta
    )
{
    return gcvSTATUS_OK;
}

