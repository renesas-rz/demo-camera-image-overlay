/*******************************************************************************
 * FILENAME: mmngr.h
 *
 * DESCRIPTION:
 *   MMNGR functions.
 *
 * PUBLIC FUNCTIONS:
 *   mmngr_alloc_nv12_dmabufs
 *   mmngr_dealloc_nv12_dmabufs
 *
 * AUTHOR: RVC       START DATE: 14/03/2023
 *
 ******************************************************************************/

#ifndef _MMNGR_H_
#define _MMNGR_H_

#include <stdio.h>
#include <stdint.h>

#include <mmngr_user_public.h>
#include <mmngr_buf_user_public.h>

/******************************************************************************
 *                            STRUCTURE DEFINITION                            *
 ******************************************************************************/

/* For dmabuf exported by mmngr */
typedef struct
{
    /* Export ID */
    int dmabuf_id;

    /* File descriptor of dmabuf */
    int dmabuf_fd;

    /* Buffer in the virtual address space of this process */
    char * p_virt_addr;

    /* Buffer's size */
    size_t size;

} mmngr_dmabuf_exp_t;

/* For buffers of input port */
typedef struct
{
    /* ID of allocated memory */
    MMNGR_ID mmngr_id;

    /* Buffer's size */
    size_t size;

    /* Physical address whose 12-bit address is shifted to the right */
    unsigned long phy_addr;

    /* Address for HW IP of allocated memory */
    unsigned long hard_addr;

    /* Address for CPU of allocated memory */
    unsigned long virt_addr;

    /* Array of dmabufs.
     * Note: Might be NULL if mmngr does not export dmabufs */
    mmngr_dmabuf_exp_t * p_dmabufs;

    /* The number of elements in 'p_dmabufs' array.
     * Note: Might be 0 if mmngr does not export dmabufs */
    uint32_t count;

} mmngr_buf_t;

/******************************************************************************
 *                            FUNCTION DECLARATION                            *
 ******************************************************************************/

/* Allocate NV12 buffers and export dmabufs.
 * Return an array of 'mmngr_buf_t' structs.
 * Note: The array must be freed when no longer used */
mmngr_buf_t * mmngr_alloc_nv12_dmabufs(uint32_t count, size_t nv12_size);

/* Deallocate dmabufs (allocated by 'mmngr_alloc_nv12_dmabufs') */
void mmngr_dealloc_nv12_dmabufs(mmngr_buf_t * p_bufs, uint32_t count);

#endif /* _MMNGR_H_ */
