/****************************************************************************
*
*    The MIT License (MIT)
*
*    Copyright (c) 2014 - 2017 Vivante Corporation
*
*    Permission is hereby granted, free of charge, to any person obtaining a
*    copy of this software and associated documentation files (the "Software"),
*    to deal in the Software without restriction, including without limitation
*    the rights to use, copy, modify, merge, publish, distribute, sublicense,
*    and/or sell copies of the Software, and to permit persons to whom the
*    Software is furnished to do so, subject to the following conditions:
*
*    The above copyright notice and this permission notice shall be included in
*    all copies or substantial portions of the Software.
*
*    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
*    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
*    DEALINGS IN THE SOFTWARE.
*
*****************************************************************************
*
*    The GPL License (GPL)
*
*    Copyright (C) 2014 - 2017 Vivante Corporation
*
*    This program is free software; you can redistribute it and/or
*    modify it under the terms of the GNU General Public License
*    as published by the Free Software Foundation; either version 2
*    of the License, or (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program; if not, write to the Free Software Foundation,
*    Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*****************************************************************************
*
*    Note: This software is released under dual MIT and GPL licenses. A
*    recipient may use this file under the terms of either the MIT license or
*    GPL License. If you wish to use only one license not the other, you can
*    indicate your decision by deleting one of the above license notices in your
*    version of this file.
*
*****************************************************************************/


#include "gc_hal_kernel_linux.h"
#include "gc_hal_kernel_allocator.h"
#include <linux/pagemap.h>
#include <linux/seq_file.h>
#include <linux/mman.h>
#include <asm/atomic.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
#include <linux/anon_inodes.h>
#endif
#include <linux/file.h>

#include "gc_hal_kernel_allocator_array.h"
#include "gc_hal_kernel_platform.h"

#define _GC_OBJ_ZONE    gcvZONE_OS


/******************************************************************************\
******************************** Debugfs Support *******************************
\******************************************************************************/

static gceSTATUS
_AllocatorDebugfsInit(
    IN gckOS Os
    )
{
    gceSTATUS status;
    gckGALDEVICE device = Os->device;

    gckDEBUGFS_DIR dir = &Os->allocatorDebugfsDir;

    gcmkONERROR(gckDEBUGFS_DIR_Init(dir, device->debugfsDir.root, "allocators"));

    return gcvSTATUS_OK;

OnError:
    return status;
}

static void
_AllocatorDebugfsCleanup(
    IN gckOS Os
    )
{
    gckDEBUGFS_DIR dir = &Os->allocatorDebugfsDir;

    gckDEBUGFS_DIR_Deinit(dir);
}

/***************************************************************************\
************************ Allocator management *******************************
\***************************************************************************/

gceSTATUS
gckOS_ImportAllocators(
    gckOS Os
    )
{
    gceSTATUS status;
    gctUINT i;
    gckALLOCATOR allocator;

    _AllocatorDebugfsInit(Os);

    INIT_LIST_HEAD(&Os->allocatorList);

    for (i = 0; i < gcmCOUNTOF(allocatorArray); i++)
    {
        if (allocatorArray[i].construct)
        {
            /* Construct allocator. */
            status = allocatorArray[i].construct(Os, &Os->allocatorDebugfsDir, &allocator);

            if (gcmIS_ERROR(status))
            {
                gcmkPRINT("["DEVICE_NAME"]: Can't construct allocator(%s)",
                          allocatorArray[i].name);

                continue;
            }

            allocator->name = allocatorArray[i].name;

            list_add_tail(&allocator->link, &Os->allocatorList);
        }
    }

#if gcdDEBUG
    list_for_each_entry(allocator, &Os->allocatorList, link)
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_WARNING, gcvZONE_OS,
            "%s(%d) Allocator: %s",
            __FUNCTION__, __LINE__,
            allocator->name
            );
    }
#endif

    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_FreeAllocators(
    gckOS Os
    )
{
    gckALLOCATOR allocator;
    gckALLOCATOR temp;

    list_for_each_entry_safe(allocator, temp, &Os->allocatorList, link)
    {
        list_del(&allocator->link);

        /* Destroy allocator. */
        allocator->destructor(allocator);
    }

    _AllocatorDebugfsCleanup(Os);

    return gcvSTATUS_OK;
}

