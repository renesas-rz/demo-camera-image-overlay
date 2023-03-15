/*******************************************************************************
 * FILENAME: v4l2.h
 *
 * DESCRIPTION:
 *   V4L2 functions.
 *
 * PUBLIC FUNCTIONS:
 *   v4l2_open_dev
 *   v4l2_verify_dev
 *
 *   v4l2_print_caps
 *   v4l2_print_format
 *   v4l2_print_framerate
 *
 *   v4l2_fourcc_to_str
 *
 *   v4l2_get_format
 *   v4l2_get_stream_params
 *   v4l2_set_format
 *   v4l2_set_framerate
 *
 *   v4l2_export_dmabuf
 *   v4l2_alloc_dmabufs
 *   v4l2_dealloc_dmabufs
 *
 *   v4l2_enqueue_buf
 *   v4l2_enqueue_bufs
 *   v4l2_dequeue_buf
 *
 *   v4l2_enable_capturing
 *   v4l2_disable_capturing
 *
 * AUTHOR: RVC       START DATE: 14/03/2023
 *
 * CHANGES:
 * 
 ******************************************************************************/

#ifndef _V4L2_H_
#define _V4L2_H_

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <linux/videodev2.h>

/******************************************************************************
 *                            STRUCTURE DEFINITION                            *
 ******************************************************************************/

/* This structure is used by V4L2 (dmabuf export mode) */
typedef struct
{
    /* File descriptor of dmabuf */
    int dmabuf_fd;

    /* Buffer in the virtual address space of this process */
    char * p_virt_addr;

    /* Buffer's size */
    size_t size;

} v4l2_dmabuf_exp_t;

/******************************************************************************
 *                            FUNCTION DECLARATION                            *
 ******************************************************************************/

/* Open V4L2 device (for example: '/dev/video0').
 * Return non-negative value if successful or -1 if error */
int v4l2_open_dev(const char * p_name);

/* Check the below conditions:
 *   - The device should support the single-planar API through the
 *     Video Capture interface.
 *   - The device should support the streaming I/O method.
 *
 * Return true if the above conditions are true. Otherwise, return false */
bool v4l2_verify_dev(int dev_fd);

/* Print capabilities of V4L2 device */
void v4l2_print_caps(int dev_fd);

/* Print current format of V4L2 device */
void v4l2_print_format(int dev_fd);

/* Print current framerate of V4L2 device */
void v4l2_print_framerate(int dev_fd);

/* Convert integer value 'fourcc' to string.
 * Return 'str' (useful when passing the function to 'printf').
 *
 * Note 1: 'fourcc' is created from macro 'v4l2_fourcc' or 'v4l2_fourcc_be':
 * https://github.com/torvalds/linux/blob/master/include/uapi/linux/videodev2.h
 *
 * Note 2: The code is based on function 'std::string fcc2s(__u32 val)':
 * https://git.linuxtv.org/v4l-utils.git/tree/utils/common/v4l2-info.cpp */
char * v4l2_fourcc_to_str(uint32_t fourcc, char str[8]);

/* Get format for V4L2 device.
 * Return true if successful. Otherwise, return false */
bool v4l2_get_format(int dev_fd, struct v4l2_format * p_fmt);

/* Get streaming parameters for V4L2 device.
 * Return true if successful. Otherwise, return false */
bool v4l2_get_stream_params(int dev_fd, struct v4l2_streamparm * p_params);

/* Set format for V4L2 device.
 * Return true if successful. Otherwise, return false */
bool v4l2_set_format(int dev_fd,
                     uint32_t img_width, uint32_t img_height,
                     uint32_t pix_fmt, enum v4l2_field field);

/* Set framerate for V4L2 device.
 * Return true if successful. Otherwise, return false */
bool v4l2_set_framerate(int dev_fd, uint32_t framerate);

/* Export dmabuf from V4L2 device.
 * Return true if successful. Otherwise, return false */
bool v4l2_export_dmabuf(int dev_fd, uint32_t index, v4l2_dmabuf_exp_t * p_buf);

/* Allocate dmabufs for V4L2 device.
 *
 * If successful, return an array of allocated dmabufs and
 * updates 'p_count' if length of the array is smaller than 'p_count'.
 *
 * If error, return NULL.
 *
 * Note: The array must be freed when no longer used */
v4l2_dmabuf_exp_t * v4l2_alloc_dmabufs(int dev_fd, uint32_t * p_count);

/* Free dmabufs (allocated by 'v4l2_alloc_dmabufs') */
void v4l2_dealloc_dmabufs(v4l2_dmabuf_exp_t * p_bufs, uint32_t count);

/* Enqueue a buffer with sequence number 'index' to V4L2 device.
 * The value 'index' ranges from zero to the number of buffers allocated with
 * the ioctl 'VIDIOC_REQBUFS' (struct 'v4l2_requestbuffers::count') minus one.
 *
 * Return true if successful. Otherwise, return false */
bool v4l2_enqueue_buf(int dev_fd, uint32_t index);

/* Enqueue buffers to V4L2 device.
 * Return true if successful. Otherwise, return false */
bool v4l2_enqueue_bufs(int dev_fd, uint32_t count);

/* Dequeue a buffer from V4L2 device.
 * Return true and update structure 'v4l2_buffer' pointed by 'p_buf' if
 * successful */
bool v4l2_dequeue_buf(int dev_fd, struct v4l2_buffer * p_buf);

/* Enable capturing process on V4L2 device.
 * Return true if successful. Otherwise, return false */
bool v4l2_enable_capturing(int dev_fd);

/* Disable capturing process on V4L2 device.
 * Return true if successful. Otherwise, return false */
bool v4l2_disable_capturing(int dev_fd);

#endif /* _V4L2_H_ */
