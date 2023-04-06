/*******************************************************************************
 * FILENAME: v4l2.h
 *
 * DESCRIPTION:
 *   V4L2 function definition.
 * 
 * NOTE:
 *   For function usage, please refer to 'v4l2.h'.
 * 
 * AUTHOR: RVC       START DATE: 14/03/2023
 *
 ******************************************************************************/

#include <fcntl.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include "v4l2.h"
#include "util.h"

/******************************************************************************
 *                            FUNCTION DEFINITION                             *
 ******************************************************************************/

int v4l2_open_dev(const char * p_name)
{
    int dev_fd = -1;
    struct stat file_st;

    /* Check parameter */
    assert(p_name != NULL);

    /* Check if 'p_name' exists or not? */
    if (stat(p_name, &file_st) == -1)
    {
        util_print_errno();
        return -1;
    }

    /* Check if 'p_name' is a special character file or not?
     * The statement is useful if 'p_name' is provided by user */
    if (S_ISCHR(file_st.st_mode) == 0)
    {
        printf("Error: '%s' is not a character special file\n", p_name);
        return -1;
    }

    /* Try to open 'p_name' */
    dev_fd = open(p_name, O_RDWR);
    if (dev_fd == -1)
    {
        util_print_errno();
    }

    return dev_fd;
}

bool v4l2_verify_dev(int dev_fd)
{
    struct v4l2_capability caps;

    /* Check parameter */
    assert(dev_fd > 0);

    /* Discover capabilities of the device:
     * https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/vidioc-querycap.html
     */
    if (ioctl(dev_fd, VIDIOC_QUERYCAP, &caps) == -1)
    {
        util_print_errno();
        return false;
    }

    /* Make sure the device supports the single-planar API through the
     * Video Capture interface */
    if (!(caps.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        printf("Error: Not a capture device\n");
        return false;
    }

    /* Make sure the device supports the streaming I/O method.
     * Otherwise, dmabuf will not work.
     *
     * https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/io.html
     * https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/mmap.html
     * https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/dmabuf.html
     * https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/vidioc-expbuf.html
     * R01US0424EJ0111_VideoCapture_UME_v1.11.pdf */
    if (!(caps.capabilities & V4L2_CAP_STREAMING))
    {
        printf("Error: Not support streaming I/O method\n");
        return false;
    }

    return true;
}

void v4l2_print_caps(int dev_fd)
{
    struct v4l2_capability caps;

    int ver_major = 0;
    int ver_minor = 0;
    int ver_steps = 0;

    /* Check parameter */
    assert(dev_fd > 0);

    /* Discover capabilities of the device */
    if (ioctl(dev_fd, VIDIOC_QUERYCAP, &caps) == -1)
    {
        util_print_errno();
    }
    else
    {
        /* Make sure the below fields are NULL-terminated */
        caps.card[31]     = '\0';
        caps.driver[15]   = '\0';
        caps.bus_info[31] = '\0';

        ver_major = (caps.version >> 16) & 0xFF;
        ver_minor = (caps.version >> 8) & 0xFF;
        ver_steps = caps.version & 0xFF;

        printf("V4L2 device:\n");
        printf("  Name: '%s'\n", caps.card);
        printf("  Bus: '%s'\n",  caps.bus_info);
        printf("  Driver: '%s (v%d.%d.%d)'\n", caps.driver, ver_major,
                                               ver_minor, ver_steps);
    }
}

void v4l2_print_format(int dev_fd)
{
    struct v4l2_format fmt;

    char fourcc_str[8] = { '\0' };
    const char * p_scan_type = NULL;

    /* Check parameter */
    assert(dev_fd > 0);

    /* Get current format of the device */
    if (v4l2_get_format(dev_fd, &fmt) == true)
    {
        /* Convert FourCC code to string */
        v4l2_fourcc_to_str(fmt.fmt.pix.pixelformat, fourcc_str);

        /* Get scan type */
        p_scan_type = (fmt.fmt.pix.field == V4L2_FIELD_NONE) ?
                      "Progressive" : "Interlaced";

        printf("V4L2 format:\n");
        printf("  Frame width (pixels): '%d' \n",  fmt.fmt.pix.width);
        printf("  Frame height (pixels): '%d' \n", fmt.fmt.pix.height);
        printf("  Bytes per line: '%d'\n",     fmt.fmt.pix.bytesperline);
        printf("  Frame size (bytes): '%d'\n", fmt.fmt.pix.sizeimage);
        printf("  Pixel format: '%s'\n", fourcc_str);
        printf("  Scan type: '%s'\n", p_scan_type);
    }
}

void v4l2_print_framerate(int dev_fd)
{
    struct v4l2_streamparm params;
    float framerate = 0;

    /* Check parameter */
    assert(dev_fd > 0);

    /* Get current streaming parameters of the device */
    if (v4l2_get_stream_params(dev_fd, &params) == true)
    {
        /* Calculate framerate */
        framerate = (1.0f * params.parm.capture.timeperframe.denominator) /
                            params.parm.capture.timeperframe.numerator;

        printf("V4L2 framerate: '%.1f'\n", framerate);
    }
}

char * v4l2_fourcc_to_str(uint32_t fourcc, char str[8])
{
    /* Check parameter */
    assert(str != NULL);

    str[0] = fourcc & 0x7f;
    str[1] = (fourcc >> 8) & 0x7f;
    str[2] = (fourcc >> 16) & 0x7f;
    str[3] = (fourcc >> 24) & 0x7f;

    if (fourcc & (1 << 31))
    {
        str[4] = '-';
        str[5] = 'B';
        str[6] = 'E';
        str[7] = '\0';
    }
    else
    {
        str[4] = '\0';
    }

    return str;
}

bool v4l2_get_format(int dev_fd, struct v4l2_format * p_fmt)
{
    struct v4l2_format fmt;

    /* Check parameters */
    assert((dev_fd > 0) && (p_fmt != NULL));

    /* Get current data format of the device.
     *
     * https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/vidioc-g-fmt.html
     * https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/pixfmt-002.html
     * https://www.kernel.org/doc/html/v4.17/media/uapi/v4l/field-order.html */
    memset(&fmt, 0, sizeof(struct v4l2_format));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl(dev_fd, VIDIOC_G_FMT, &fmt) == -1)
    {
        util_print_errno();
        return false;
    }

    /* Copy data from 'fmt' to 'p_fmt' */
    memcpy(p_fmt, &fmt, sizeof(struct v4l2_format));

    return true;
}

bool v4l2_get_stream_params(int dev_fd, struct v4l2_streamparm * p_params)
{
    struct v4l2_streamparm params;

    /* Check parameters */
    assert((dev_fd > 0) && (p_params != NULL));

    /* Get current streaming parameters of the device */
    memset(&params, 0, sizeof(struct v4l2_streamparm));
    params.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl(dev_fd, VIDIOC_G_PARM, &params) == -1)
    {
        util_print_errno();
        return false;
    }

    /* Copy data from 'params' to 'p_params' */
    memcpy(p_params, &params, sizeof(struct v4l2_streamparm));

    return true;
}

bool v4l2_set_format(int dev_fd,
                     uint32_t img_width, uint32_t img_height,
                     uint32_t pix_fmt, enum v4l2_field field)
{
    struct v4l2_format fmt;

    /* Check parameters */
    assert(dev_fd > 0);
    assert((img_width > 0) && (img_height > 0));

    /* Get current format of the device */
    if (v4l2_get_format(dev_fd, &fmt) == false)
    {
        return false;
    }

    /* Set and reload data format of the device */
    fmt.fmt.pix.width        = img_width;
    fmt.fmt.pix.height       = img_height;
    fmt.fmt.pix.pixelformat  = pix_fmt;
    fmt.fmt.pix.field        = field;

    if (ioctl(dev_fd, VIDIOC_S_FMT, &fmt) == -1)
    {
        util_print_errno();
        return false;
    }

    return true;
}

bool v4l2_set_framerate(int dev_fd, uint32_t framerate)
{
    struct v4l2_streamparm params;

    /* Check parameters */
    assert((dev_fd > 0) && (framerate > 0));

    /* Get current framerate of the device */
    if (v4l2_get_stream_params(dev_fd, &params) == false)
    {
        return false;
    }

    /* Note: Should set the framerate (1/25, 1/30...) when the flag
     * 'V4L2_CAP_TIMEPERFRAME' is set in the 'capability' field.
     *
     * https://www.kernel.org/doc/html/v5.0/media/uapi/v4l/vidioc-g-parm.html */
    if (!(params.parm.capture.capability & V4L2_CAP_TIMEPERFRAME))
    {
        printf("Error: Framerate setting is not supported\n");
        return false;
    }

    params.parm.capture.timeperframe.numerator   = 1;
    params.parm.capture.timeperframe.denominator = framerate;

    if (ioctl(dev_fd, VIDIOC_S_PARM, &params) == -1)
    {
        util_print_errno();
        return false;
    }

    return true;
}

bool v4l2_export_dmabuf(int dev_fd, uint32_t index, v4l2_dmabuf_exp_t * p_buf)
{
    char * p_virt_addr = NULL;

    struct v4l2_buffer buf;
    struct v4l2_exportbuffer expbuf;

    /* Check parameters */
    assert((dev_fd > 0) && (p_buf != NULL));

    /* Get virtual address of the buffer */
    memset(&buf, 0, sizeof(struct v4l2_buffer));
    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index  = index;

    /* https://www.kernel.org/doc/html/v5.0/media/uapi/v4l/vidioc-querybuf.html
     */
    if (ioctl(dev_fd, VIDIOC_QUERYBUF, &buf) == -1)
    {
        util_print_errno();
        return false;
    }

    /* Export the buffer as a dmabuf file descriptor */
    memset(&expbuf, 0, sizeof(struct v4l2_exportbuffer));
    expbuf.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    expbuf.index = index;

    /* https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/vidioc-expbuf.html */
    if (ioctl(dev_fd, VIDIOC_EXPBUF, &expbuf) == -1)
    {
        util_print_errno();
        return false;
    }

    /* Map 'dev_fd' into memory.
     * Notes:
     *   - 1st argument: Kernel chooses a page-aligned address at which to
     *                   create mapping.
     *   - 2nd argument: Length of the mapping.
     *   - 3rd argument: Required.
     *   - 4th argument: Recommended */
    p_virt_addr = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                       MAP_SHARED, dev_fd, buf.m.offset);

    if (p_virt_addr == MAP_FAILED)
    {
        /* Close dmabuf file */
        close(expbuf.fd);

        util_print_errno();
        return false;
    }

    p_buf->dmabuf_fd   = expbuf.fd;
    p_buf->p_virt_addr = p_virt_addr;
    p_buf->size        = buf.length;

    return true;
}

v4l2_dmabuf_exp_t * v4l2_alloc_dmabufs(int dev_fd, uint32_t * p_count)
{
    v4l2_dmabuf_exp_t * p_bufs = NULL;

    uint32_t index = 0;
    struct v4l2_requestbuffers reqbufs;

    /* Check parameters */
    assert(dev_fd > 0);
    assert((p_count != NULL) && ((*p_count) > 0));

    /* Request and allocate buffers for the device */
    memset(&reqbufs, 0, sizeof(struct v4l2_requestbuffers));
    reqbufs.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbufs.memory = V4L2_MEMORY_MMAP;
    reqbufs.count  = *p_count;

    /* https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/vidioc-reqbufs.html
     */
    if (ioctl(dev_fd, VIDIOC_REQBUFS, &reqbufs) == -1)
    {
        util_print_errno();
        return NULL;
    }

    /* Exit if the driver runs out of free memory */
    if (reqbufs.count == 0)
    {
        printf("Error: Failed to allocate buffers due to out of memory\n");
        return NULL;
    }

    /* Allocate an array of struct 'v4l2_dmabuf_exp_t' */
    p_bufs = (v4l2_dmabuf_exp_t *)
             malloc(reqbufs.count * sizeof(v4l2_dmabuf_exp_t));

    /* Export dmabufs */
    for (index = 0; index < reqbufs.count; index++)
    {
        if (v4l2_export_dmabuf(dev_fd, index, p_bufs + index) == false)
        {
            break;
        }
    }

    if (index < reqbufs.count)
    {
        printf("Error: Failed to export dmabuf at buffer index '%d'\n", index);

        v4l2_dealloc_dmabufs(p_bufs, index);
        return NULL;
    }

    /* Update the actual number of buffers allocated */
    *p_count = reqbufs.count;

    return p_bufs;
}

void v4l2_dealloc_dmabufs(v4l2_dmabuf_exp_t * p_bufs, uint32_t count)
{
    uint32_t index = 0;

    /* Check parameter */
    assert(p_bufs != NULL);

    for (index = 0; index < count; index++)
    {
        /* It is recommended to close a dmabuf file when it is no longer
         * used to allow the associated memory to be reclaimed */
        close((p_bufs + index)->dmabuf_fd);

        /* Unmap pages of memory */
        munmap((p_bufs + index)->p_virt_addr, (p_bufs + index)->size);
    }

    /* Free entire array */
    free(p_bufs);
}

bool v4l2_enqueue_buf(int dev_fd, uint32_t index)
{
    bool b_is_success = true;
    struct v4l2_buffer buf;

    /* Check parameter */
    assert(dev_fd > 0);

    memset(&buf, 0, sizeof(struct v4l2_buffer));
    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index  = index;

    /* https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/vidioc-qbuf.html
     */
    if (ioctl(dev_fd, VIDIOC_QBUF, &buf) == -1)
    {
        util_print_errno();
        b_is_success = false;
    }

    return b_is_success;
}

bool v4l2_enqueue_bufs(int dev_fd, uint32_t count)
{
    uint32_t index = 0;
    bool b_is_success = true;

    /* Check parameter */
    assert(dev_fd > 0);

    for (index = 0; index < count; index++)
    {
        if (v4l2_enqueue_buf(dev_fd, index) == false)
        {
            b_is_success = false;
            break;
        }
    }

    return b_is_success;
}

bool v4l2_dequeue_buf(int dev_fd, struct v4l2_buffer * p_buf)
{
    struct v4l2_buffer buf;

    /* Check parameters */
    assert(dev_fd > 0);
    assert(p_buf != NULL);

    /* Dequeue the filled buffer from the driver's outgoing process.
     *
     * Note: By default, 'VIDIOC_DQBUF' blocks when no buffer is in the
     * outgoing queue. When the 'O_NONBLOCK' flag was given to the 'open()'
     * function, 'VIDIOC_DQBUF' returns immediately with an 'EAGAIN' error
     * code when no buffer is available.
     *
     * https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/vidioc-qbuf.html
     */
    memset(&buf, 0, sizeof(struct v4l2_buffer));
    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (ioctl(dev_fd, VIDIOC_DQBUF, &buf) == -1)
    {
        util_print_errno();
        return false;
    }

    *p_buf = buf;
    return true;
}

bool v4l2_enable_capturing(int dev_fd)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    /* Check parameter */
    assert(dev_fd > 0);

    /* Start streaming I/O:
     * https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/vidioc-streamon.html
     */
    if (ioctl(dev_fd, VIDIOC_STREAMON, &type) == -1)
    {
        util_print_errno();
        return false;
    }

    return true;
}

bool v4l2_disable_capturing(int dev_fd)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    /* Check parameter */
    assert(dev_fd > 0);

    /* Stop streaming I/O:
     * https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/vidioc-streamon.html
     */
    if (ioctl(dev_fd, VIDIOC_STREAMOFF, &type) == -1)
    {
        util_print_errno();
        return false;
    }

    return true;
}
