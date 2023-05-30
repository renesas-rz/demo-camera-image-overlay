/* Copyright (c) 2023 Renesas Electronics Corp.
 * SPDX-License-Identifier: MIT-0 */

/*******************************************************************************
 * FILENAME: mmngr.c
 *
 * DESCRIPTION:
 *   MMNGR function definition.
 * 
 * NOTE:
 *   For function usage, please refer to 'mmngr.h'.
 * 
 * AUTHOR: RVC       START DATE: 14/03/2023
 *
 ******************************************************************************/

#include <assert.h>
#include <stdlib.h>

#include "util.h"
#include "mmngr.h"

/******************************************************************************
 *                            FUNCTION DEFINITION                             *
 ******************************************************************************/

mmngr_buf_t * mmngr_alloc_nv12_dmabufs(uint32_t count, size_t nv12_size)
{
    mmngr_buf_t * p_bufs = NULL;

    int b_alloc_ok = R_MM_OK;

    size_t plane0_size = 0;
    size_t plane1_size = 0;

    uint32_t index = 0;

    /* Check parameters */
    assert((count > 0) && (nv12_size > 0));

    /* Get size of plane 0 and plane 1 of NV12 buffer */
    plane0_size = (2.0f * nv12_size) / 3;
    plane1_size = util_get_page_aligned_size((1.0f * nv12_size) / 3);

    /* Exit if size of plane 0 is not aligned to page size */
    if (!util_is_aligned_to_page_size(plane0_size))
    {
        printf("Error: Size of plane 0 is not aligned to page size\n");
        return NULL;
    }

    /* Allocate array of struct 'mmngr_buf_t' */
    p_bufs = (mmngr_buf_t *)malloc(count * sizeof(mmngr_buf_t));

    for (index = 0; index < count; index++)
    {
        /* Allocate memory space */
        b_alloc_ok = mmngr_alloc_in_user(&(p_bufs[index].mmngr_id),
                                         plane0_size + plane1_size,
                                         &(p_bufs[index].phy_addr),
                                         &(p_bufs[index].hard_addr),
                                         &(p_bufs[index].virt_addr),
                                         MMNGR_VA_SUPPORT);

        /* Maybe it's out of memory */
        if (b_alloc_ok != R_MM_OK)
        {
            printf("Error: MMNGR failed to allocate memory space\n");
            break;
        }

        /* Set buffer's size */
        p_bufs[index].size = nv12_size;

        /* Allocate array of 'mmngr_dmabuf_exp_t' */
        p_bufs[index].count = 2;
        p_bufs[index].p_dmabufs = (mmngr_dmabuf_exp_t *)
                                  malloc(2 * sizeof(mmngr_dmabuf_exp_t));

        /* Export dmabuf for plane 0 */
        p_bufs[index].p_dmabufs[0].size = plane0_size;
        p_bufs[index].p_dmabufs[0].p_virt_addr = (char *)
                                                 p_bufs[index].virt_addr;

        mmngr_export_start_in_user(&(p_bufs[index].p_dmabufs[0].dmabuf_id),
                                   plane0_size,
                                   p_bufs[index].hard_addr,
                                   &(p_bufs[index].p_dmabufs[0].dmabuf_fd));

        /* Export dmabuf for plane 1 */
        p_bufs[index].p_dmabufs[1].size = (1.0f * nv12_size) / 3;
        p_bufs[index].p_dmabufs[1].p_virt_addr = (char *)
                                                 (p_bufs[index].virt_addr +
                                                  plane0_size);

        mmngr_export_start_in_user(&(p_bufs[index].p_dmabufs[1].dmabuf_id),
                                   plane1_size,
                                   p_bufs[index].hard_addr + plane0_size,
                                   &(p_bufs[index].p_dmabufs[1].dmabuf_fd));
    }

    if (index < count)
    {
        mmngr_dealloc_nv12_dmabufs(p_bufs, index);
        return NULL;
    }

    return p_bufs;
}

void mmngr_dealloc_nv12_dmabufs(mmngr_buf_t * p_bufs, uint32_t count)
{
    uint32_t index = 0;

    /* Check parameter */
    assert(p_bufs != NULL);

    for (index = 0; index < count; index++)
    {
        /* Stop exporting dmabuf for plane 0 */
        mmngr_export_end_in_user(p_bufs[index].p_dmabufs[0].dmabuf_id);

        /* Stop exporting dmabuf for plane 1 */
        mmngr_export_end_in_user(p_bufs[index].p_dmabufs[1].dmabuf_id);

        /* Deallocate array of 'mmngr_dmabuf_exp_t' */
        free(p_bufs[index].p_dmabufs);

        /* Deallocate memory space */
        mmngr_free_in_user(p_bufs[index].mmngr_id);
    }

    /* Deallocate array of struct 'mmngr_buf_t' */
    free(p_bufs);
}
