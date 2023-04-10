#include <math.h>
#include <fcntl.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <semaphore.h>

#include <wayland-client.h>
#include <wayland-egl.h>

#include <drm/drm_fourcc.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglplatform.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl32.h>

#include <mmngr_user_public.h>
#include <mmngr_buf_user_public.h>

#include <OMX_Core.h>
#include <OMX_Types.h>
#include <OMX_Component.h>

#include <OMXR_Extension_vecmn.h>
#include <OMXR_Extension_h264e.h>

#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <linux/videodev2.h>

/* For YUYV */
#define YUYV_FRAME_WIDTH_IN_PIXELS  640
#define YUYV_FRAME_HEIGHT_IN_PIXELS 480

#define YUYV_FRAME_WIDTH_IN_BYTES (YUYV_FRAME_WIDTH_IN_PIXELS * 2)
#define YUYV_FRAME_SIZE_IN_BYTES  (YUYV_FRAME_WIDTH_IN_PIXELS  * \
                                   YUYV_FRAME_HEIGHT_IN_PIXELS * 2)

/* For NV12 */
#define NV12_FRAME_WIDTH_IN_PIXELS  YUYV_FRAME_WIDTH_IN_PIXELS
#define NV12_FRAME_HEIGHT_IN_PIXELS YUYV_FRAME_HEIGHT_IN_PIXELS

#define NV12_FRAME_SIZE_IN_BYTES (NV12_FRAME_WIDTH_IN_PIXELS  * \
                                  NV12_FRAME_HEIGHT_IN_PIXELS * 1.5f)

/* For NV12 (plane 0) */
#define NV12_PLANE0_WIDTH_IN_PIXELS  NV12_FRAME_WIDTH_IN_PIXELS
#define NV12_PLANE0_HEIGHT_IN_PIXELS NV12_FRAME_HEIGHT_IN_PIXELS

#define NV12_PLANE0_SIZE_IN_BYTES (NV12_PLANE0_WIDTH_IN_PIXELS  * \
                                   NV12_PLANE0_HEIGHT_IN_PIXELS * 1)

/* For NV12 (plane 1) */
#define NV12_PLANE1_WIDTH_IN_PIXELS  (NV12_FRAME_WIDTH_IN_PIXELS  / 2)
#define NV12_PLANE1_HEIGHT_IN_PIXELS (NV12_FRAME_HEIGHT_IN_PIXELS / 2)

#define NV12_PLANE1_SIZE_IN_BYTES (NV12_PLANE1_WIDTH_IN_PIXELS  * \
                                   NV12_PLANE1_HEIGHT_IN_PIXELS * 2)

/* For V4L2 */

/* The sample app uses C270 HD Webcam */
#define V4L2_USB_CAMERA    "/dev/video0"
#define V4L2_REQUESTED_FPS 24

/* For OMX */

/* The component name for H.264 encoder media component */
#define RENESAS_VIDEO_ENCODER_NAME "OMX.RENESAS.VIDEO.ENCODER.H264"

/* The macro will be set to input port:
 *   OMX_PARAM_PORTDEFINITIONTYPE::format::video::nFrameWidth
 *   (OMX_VIDEO_PORTDEFINITIONTYPE::nFrameWidth)
 *
 * According to OMX IL specification 1.1.2, 'nFrameWidth'
 * is the width of the data in pixels.
 *
 * According to document 'R01USxxxxEJxxxx_vecmn_v1.0.pdf',
 * if 'nFrameWidth' is an odd value, it will be rounded down to the closest
 * even value (*).
 *
 * According to document 'R01USxxxxEJxxxx_h264e_v1.0.pdf',
 * if 'nFrameWidth' is not a multiple of 16, the encoder automatically
 * calculates and adds the left crop information to the encoded stream (**).
 *
 * Warning: Rules (*) and (**) are correct in decoding process, but
 * they are not confirmed in encoding process */
#define OMX_INPUT_FRAME_WIDTH  NV12_FRAME_WIDTH_IN_PIXELS

/* The macro will be set to input port:
 *   OMX_PARAM_PORTDEFINITIONTYPE::format::video::nFrameHeight
 *   (OMX_VIDEO_PORTDEFINITIONTYPE::nFrameHeight)
 *
 * According to OMX IL specification 1.1.2, 'nFrameHeight'
 * is the height of the data in pixels.
 *
 * According to document 'R01USxxxxEJxxxx_vecmn_v1.0.pdf',
 * if 'nFrameHeight' is an odd value, it will be rounded down to the closest
 * even value (*).
 *
 * According to document 'R01USxxxxEJxxxx_h264e_v1.0.pdf',
 * if 'nFrameHeight' is not a multiple of 16, the encoder automatically
 * calculates and adds the bottom crop information to the encoded stream (**).
 *
 * Warning: Rules (*) and (**) are not confirmed in encoding process */
#define OMX_INPUT_FRAME_HEIGHT  NV12_FRAME_HEIGHT_IN_PIXELS

/* The macro will be converted to Q16 format and then set to input port:
 *   OMX_PARAM_PORTDEFINITIONTYPE::format::video::xFramerate
 *   (OMX_VIDEO_PORTDEFINITIONTYPE::xFramerate)
 *
 * According to OMX IL specification 1.1.2, 'xFramerate' is the
 * frame rate whose unit is frames per second. The value is represented
 * in Q16 format and used on the port which handles uncompressed data.
 *
 * Note: For the list of supported 'xFramerate' values, please refer to
 * 'Table 6-5' in document 'R01USxxxxEJxxxx_h264e_v1.0.pdf' */
#define OMX_INPUT_FRAME_RATE 24 /* FPS */

/* The macro will be set to input port:
 *   OMX_PARAM_PORTDEFINITIONTYPE::format::video::nStride
 *   (OMX_VIDEO_PORTDEFINITIONTYPE::nStride)
 *
 * Basically, 'nStride' is the sum of 'nFrameWidth' and extra padding
 * pixels at the end of each row of a frame.
 *
 * According to document 'R01USxxxxEJxxxx_vecmn_v1.0.pdf':
 *   - If 'eColorFormat' is 'OMX_COLOR_FormatYUV420SemiPlanar', 'nStride'
 *     must be a multiple of 32 (*).
 *   - If the 'nFrameWidth' exceeds 'nStride', 'OMX_SetParameter' returns
 *     an error.
 *
 * According to document 'R01USxxxxEJxxxx_h264e_v1.0.pdf', the required
 * memory size for buffers of the input port is:
 *   ('nStride' * 'nSlideHeight' * 1.5) * 'nBufferCountActual'
 * instead of:
 *   ('nFrameWidth' * 'nFrameHeight' * 1.5) * 'nBufferCountActual'
 *
 * However, in some cases, 'nStride' is equal to 'nFrameWidth' and
 * 'nSliceHeight' is equal to 'nFrameHeight'.
 *
 * Examples (from section 8.1 in document 'R01USxxxxEJxxxx_h264e_v1.0.pdf'):
 *   1. If 'nFrameWidth' is 720 and
 *      'eColorFormat' is 'OMX_COLOR_FormatYUV420SemiPlanar',
 *      'nStride' will be 736 (even value and is a multiple of 32).
 *   2. If 'nFrameWidth' is 1920 and
 *      'eColorFormat' is 'OMX_COLOR_FormatYUV420SemiPlanar',
 *      'nStride' will be 1920 (even value and is a multiple of 32).
 *
 * Note: 'nStride' plays an important role in the layout of buffer data which
 * can be seen in 'Figure 6-6' of document 'R01USxxxxEJxxxx_vecmn_v1.0.pdf'.
 *
 * TODO: The macro should be calculated based on 'nFrameWidth' and
 * 'eColorFormat' instead of constant value.
 *
 * Warning: Rule (*) is not confirmed in encoding process */
#define OMX_INPUT_STRIDE  OMX_INPUT_FRAME_WIDTH /* pixels per row */

/* The macro will be set to input port:
 *   OMX_PARAM_PORTDEFINITIONTYPE::format::video::nSliceHeight
 *   (OMX_VIDEO_PORTDEFINITIONTYPE::nSliceHeight)
 *
 * Basically, 'nSliceHeight' is the sum of 'nFrameHeight' and extra
 * padding pixels at the end of each column of a frame.
 *
 * According to document 'R01USxxxxEJxxxx_vecmn_v1.0.pdf':
 *   - If 'nSliceHeight' is an odd value, it will be rounded down to the
 *     closest even value (*).
 *   - If the 'nFrameHeight' exceeds 'nSliceHeight', 'OMX_SetParameter'
 *     returns an error.
 *
 * Examples (from section 8.1 in document 'R01USxxxxEJxxxx_h264e_v1.0.pdf'):
 *   1. If 'nFrameHeight' is 480, 'nSliceHeight' will be 480 (even value).
 *   2. If 'nFrameHeight' is 1080, 'nSliceHeight' will be 1080 (even value).
 *
 * Note: 'nSliceHeight' plays an important role in the layout of buffer data
 * (of the input port) which can be seen in 'Figure 6-6' of document
 * 'R01USxxxxEJxxxx_vecmn_v1.0.pdf'.
 *
 * Basically, 'nSliceHeight' and 'nStride' are used to calculate start address
 * of plane 1 (UV plane) of NV12 (as below):
 *
 *   'plane1_start_addr' = 'plane0_start_addr' + ('nStride' * 'nSliceHeight')
 *
 *     1. 'plane0_start_addr' is the start address of plane 0 (Y plane) of NV12.
 *        It should be divisible by the page size (4096 bytes).
 *
 *        Note: Function 'int mmngr_alloc_in_user(
 *                            MMNGR_ID *pid,                    (output)
 *                            unsigned long size,               (input)
 *                            unsigned long *pphy_addr,         (output)
 *                            unsigned long *phard_addr,        (output)
 *                            unsigned long *puser_virt_addr,   (output)
 *                            unsigned long flag                (input)
 *                        )'
 *        is one of the suitable choices because pointers 'pphy_addr',
 *        'phard_addr', and 'puser_virt_addr' are divisible by the page size
 *        (see document 'RCH2M2_MMP_MMNGR_Linux_UME_140.pdf').
 *
 *     2. 'plane1_start_addr' is the start address of plane 1 (UV plane) of
 *        NV12. Unlike plane 0, it does not need to be divisible by the page
 *        size in encoding process.
 *
 * TODO: The macro should be calculated based on 'nFrameHeight' instead of
 * constant value.
 *
 * Warning: Rule (*) is not confirmed in encoding process */
#define OMX_INPUT_SLICE_HEIGHT  OMX_INPUT_FRAME_HEIGHT /* pixels per column */

/* The macro will be set to input port:
 *   OMX_PARAM_PORTDEFINITIONTYPE::format::video::eColorFormat
 *   (OMX_VIDEO_PORTDEFINITIONTYPE::eColorFormat)
 *
 * According to OMX IL specification 1.1.2, 'eColorFormat' is the
 * color format of the data of the input port.
 *
 * Note: On G2L, 'eColorFormat' will always be
 * 'OMX_COLOR_FormatYUV420SemiPlanar' which represents NV12 format */
#define OMX_INPUT_COLOR_FORMAT  OMX_COLOR_FormatYUV420SemiPlanar

/* The macro will be set to output port:
 *   OMX_PARAM_PORTDEFINITIONTYPE::format::video::nBitrate
 *   (OMX_VIDEO_PORTDEFINITIONTYPE::nBitrate)
 *
 * According to OMX IL specification 1.1.2, 'nBitrate' is the bit rate in bits
 * per second of the frame to be used on the port which handles compressed
 * data */
#define OMX_OUTPUT_BIT_RATE 1572864 /* 1.5 Mbit/s */

/* The macro will be set to output port:
 *   OMX_PARAM_PORTDEFINITIONTYPE::format::video::eCompressionFormat
 *   (OMX_VIDEO_PORTDEFINITIONTYPE::eCompressionFormat)
 *
 * According to document 'R01USxxxxEJxxxx_h264e_v1.0.pdf',
 * 'eCompressionFormat' only accepts value 'OMX_VIDEO_CodingAVC' */
#define OMX_OUTPUT_COMPRESSION_FORMAT  OMX_VIDEO_CodingAVC

#define OMX_INIT_STRUCTURE(p_struct)                                \
{                                                                   \
    memset((p_struct), 0, sizeof(*(p_struct)));                     \
                                                                    \
    (p_struct)->nSize = sizeof(*(p_struct));                        \
                                                                    \
    (p_struct)->nVersion.s.nVersionMajor = OMX_VERSION_MAJOR;       \
    (p_struct)->nVersion.s.nVersionMinor = OMX_VERSION_MINOR;       \
    (p_struct)->nVersion.s.nRevision     = OMX_VERSION_REVISION;    \
    (p_struct)->nVersion.s.nStep         = OMX_VERSION_STEP;        \
}

/* For EGL and OpenGL ES */
typedef EGLImageKHR (*EGLCREATEIMAGEKHR) (EGLDisplay dpy, EGLContext ctx,
                                          EGLenum target,
                                          EGLClientBuffer buffer,
                                          EGLint * p_attr_list);

typedef EGLBoolean (*EGLDESTROYIMAGEKHR) (EGLDisplay dpy, EGLImageKHR image);

typedef void (*GLEGLIMAGETARGETTEXTURE2DOES) (GLenum target,
                                              GLeglImageOES image);

/* For sharing data between threads and callbacks related to OMX */
typedef struct
{
    /* The semaphore structures are used to confirm the completion of
     * state transition and the completion of reception of H.264 data */
    sem_t sem_event_idle;       /* The component is in state IDLE */
    sem_t sem_event_exec;       /* The component is in state EXECUTING */
    sem_t sem_h264_hdr_done;    /* The SPS NAL and PPS NAL are received */
    sem_t sem_h264_data_done;   /* The Data NAL is received */

    /* The handle is used to access all the public methods of media
     * component */
    OMX_HANDLETYPE video_enc_handle;

    /* The structures contain a set of generic fields that characterize each
     * port of the media component */
    OMX_PARAM_PORTDEFINITIONTYPE in_port;  /* Input port */
    OMX_PARAM_PORTDEFINITIONTYPE out_port; /* Output port */

    /* A buffer for input port.
     *
     * TODO: Need to allocate an array of buffers based on
     * 'OMX_PARAM_PORTDEFINITIONTYPE::nBufferCountActual' */
    OMX_BUFFERHEADERTYPE * p_in_buf;

    /* Buffers for output port.
     *
     * Note: Size of 'pp_out_bufs' is defined in
     * 'OMX_PARAM_PORTDEFINITIONTYPE::nBufferCountActual' */
    OMX_BUFFERHEADERTYPE ** pp_out_bufs;

    /* File descriptor for storing H.264 data */
    FILE * p_h264_fd;

} shared_data_t;

/* For dummy buffers of input port */
typedef struct
{
    /* ID of allocated memory */
    MMNGR_ID mmngr_id;

    /* Physical address whose 12-bit address is shifted to the right */
    unsigned long phy_addr;

    /* Address for HW IP of allocated memory */
    unsigned long hard_addr;

    /* Address for CPU of allocated memory */
    unsigned long user_virt_addr;

} mmngr_buf_t;

/* Function declarations */

/* Get smallest integral value not less than 'size' and
 * is a multiple of page size (4096 bytes).
 */
size_t page_size_get_aligned_size(size_t size);

/* Check if 'size' is aligned to page size or not?
 */
bool page_size_is_size_aligned(size_t size);

/*
 * Read file's contents.
 * Note: The content must be freed when no longer used.
 */
char * file_read_str(const char * p_filename);

/*
 * Write data to a file.
 * Note: The data is not freed by this function.
 */
void file_write_buffer(const char * p_filename,
                       const char * p_buffer, size_t size);

/*
 * Compile the shader from file 'p_filename', with error handling.
 */
GLuint shader_create(const char * p_filename, GLenum type);

/*
 * Create program from 2 existing vertex shader object and
 * fragment shader object.
 */
GLuint program_create(GLuint vertex_shader_object,
                      GLuint fragment_shader_object);

/* The method is used to notify the application when an event of interest
 * occurs within the component.
 *
 * Events are defined in the 'OMX_EVENTTYPE' enumeration.
 * Please see that enumeration for details of what will be returned for
 * each type of event.
 *
 * Callbacks should not return an error to the component, so if an error
 * occurs, the application shall handle it internally.
 *
 * Note: This is a blocking call.
 */
OMX_ERRORTYPE omx_event_handler(OMX_HANDLETYPE hComponent, OMX_PTR pAppData,
                                OMX_EVENTTYPE eEvent, OMX_U32 nData1,
                                OMX_U32 nData2, OMX_PTR pEventData);

/* This method is used to return emptied buffers from an input port back to
 * the application for reuse.
 *
 * This is a blocking call so the application should not attempt to refill
 * the buffers during this call, but should queue them and refill them in
 * another thread.
 *
 * Callbacks should not return an error to the component, so the application
 * shall handle any errors generated internally.
 */
OMX_ERRORTYPE omx_empty_buffer_done(OMX_HANDLETYPE hComponent, OMX_PTR pAppData,
                                    OMX_BUFFERHEADERTYPE* pBuffer);

/* The method is used to return filled buffers from an output port back to
 * the application for emptying and then reuse.
 *
 * This is a blocking call so the application should not attempt to empty
 * the buffers during this call, but should queue them and empty them in
 * another thread.
 *
 * Callbacks should not return an error to the component, so the application
 * shall handle any errors generated internally.
 */
OMX_ERRORTYPE omx_fill_buffer_done(OMX_HANDLETYPE hComponent, OMX_PTR pAppData,
                                   OMX_BUFFERHEADERTYPE* pBuffer);

/* This method blocks calling thread until the component is in state 'state' */
OMX_BOOL omx_wait_state(OMX_HANDLETYPE handle, OMX_STATETYPE state);

int main(int argc, char ** pp_argv)
{
    /***************************
     * Step 1: Open USB camera *
     ***************************/

    int usb_cam_fd = -1;
    struct stat usb_cam_st;

    struct v4l2_buffer usb_cam_buf;
    struct v4l2_format usb_cam_fmt;
    struct v4l2_capability usb_cam_caps;
    struct v4l2_exportbuffer usb_cam_expbuf;
    struct v4l2_streamparm usb_cam_streamparm;
    struct v4l2_requestbuffers usb_cam_reqbufs;

    enum v4l2_buf_type usb_cam_buf_type;

    struct v4l2_fract * p_usb_cam_fract = NULL;

    char * p_yuyv_user_virt_addr = NULL;
    int yuyv_dmabuf_fd = -1;

    /* Exit program if size of YUYV frame is not aligned to page size
     *
     * Mali library requires that both address and size of dmabuf must be
     * multiples of page size.
     *
     * If a frame is in NV12 format (size: 640x480), then plane 1 (UV plane)
     * will be 153,600 bytes.
     * Since this size is not a multiple of page size, the Mali library will
     * output the following messages when dmabuf of plane 1 is imported to it:
     *
     *  [   28.144983] sg_dma_len(s)=153600 is not a multiple of PAGE_SIZE
     *  [   28.151050] WARNING: CPU: 1 PID: 273 at mali_kbase_mem_linux.c:1184
     *                 kbase_mem_umm_map_attachment+0x1a8/0x270 [mali_kbase]
     *  ... */
    assert(page_size_is_size_aligned(YUYV_FRAME_SIZE_IN_BYTES));

    /* Check if 'V4L2_USB_CAMERA' exists or not? */
    assert(stat(V4L2_USB_CAMERA, &usb_cam_st) == 0);

    /* Check if 'V4L2_USB_CAMERA' is a special character file or not?
     * The statement is useful if 'V4L2_USB_CAMERA' is provided by user */
    assert(S_ISCHR(usb_cam_st.st_mode));

    /* Try to open 'V4L2_USB_CAMERA' */
    usb_cam_fd = open(V4L2_USB_CAMERA, O_RDWR);
    assert(usb_cam_fd != -1);

    /*********************************
     * Step 2: Verify the USB camera *
     *********************************/

    /* Discover capabilities of 'V4L2_USB_CAMERA'.
     * Link: https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/
     *       vidioc-querycap.html */
    assert(ioctl(usb_cam_fd, VIDIOC_QUERYCAP, &usb_cam_caps) == 0);

    /* Make sure the USB camera supports the single-planar API through the
     * Video Capture interface */
    assert(usb_cam_caps.capabilities & V4L2_CAP_VIDEO_CAPTURE);

    /* Print name and bus information of the USB camera */
    printf("USB camera: '%s' (bus: '%s')\n", usb_cam_caps.card,
                                             usb_cam_caps.bus_info);

    /* Make sure the USB camera supports the streaming I/O method.
     * Otherwise, dmabuf will not work.
     * Links:
     *  1. https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/io.html
     *  2. https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/mmap.html
     *  3. https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/dmabuf.html
     *  4. https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/
     *     vidioc-expbuf.html
     *  5. R01US0424EJ0111_VideoCapture_UME_v1.11.pdf */
    assert(usb_cam_caps.capabilities & V4L2_CAP_STREAMING);

    /* Get current data format of the USB camera.
     * Links:
     *   1. https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/
     *      vidioc-g-fmt.html
     *   2. https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/
     *      pixfmt-002.html
     *   3. https://www.kernel.org/doc/html/v4.17/media/uapi/v4l/
     *      field-order.html */
    memset(&usb_cam_fmt, 0, sizeof(struct v4l2_format));
    usb_cam_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    /* Enumeration and macro values for 'struct v4l2_format' can be found at:
     *   https://github.com/torvalds/linux/blob/master/include/uapi/linux/
     *   videodev2.h */
    assert(ioctl(usb_cam_fd, VIDIOC_G_FMT, &usb_cam_fmt) == 0);

    /* Set and reload data format of the USB camera */
    usb_cam_fmt.fmt.pix.width = YUYV_FRAME_WIDTH_IN_PIXELS;
    usb_cam_fmt.fmt.pix.height = YUYV_FRAME_HEIGHT_IN_PIXELS;
    usb_cam_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV; /* YUV 4:2:2 */
    usb_cam_fmt.fmt.pix.field = V4L2_FIELD_NONE; /* Progressive format */

    assert(ioctl(usb_cam_fd, VIDIOC_S_FMT, &usb_cam_fmt) == 0);

    /* Confirm the number of bytes per line of YUYV format */
    assert(usb_cam_fmt.fmt.pix.bytesperline == YUYV_FRAME_WIDTH_IN_BYTES);

    /* Confirm the size of YUYV frame */
    assert(usb_cam_fmt.fmt.pix.sizeimage == YUYV_FRAME_SIZE_IN_BYTES);

    /* Confirm the pixel format should be YUYV */
    assert(usb_cam_fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV);

    /* Confirm the video frames should be in progressive format */
    assert(usb_cam_fmt.fmt.pix.field == V4L2_FIELD_NONE);

    /* Get current framerate of the USB camera */
    memset(&usb_cam_streamparm, 0, sizeof(struct v4l2_streamparm));
    usb_cam_streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    assert(ioctl(usb_cam_fd, VIDIOC_G_PARM, &usb_cam_streamparm) == 0);

    p_usb_cam_fract = &(usb_cam_streamparm.parm.capture.timeperframe);

    /* Note: Should set the framerate (1/25, 1/30...) when the flag
     * 'V4L2_CAP_TIMEPERFRAME' is set in the 'capability' field.
     *
     * Links:
     *   1. https://www.kernel.org/doc/html/v5.0/media/uapi/v4l/
     *      vidioc-g-parm.html
     *   2. https://github.com/GStreamer/gst-plugins-good/blob/master/sys/
     *      v4l2/gstv4l2object.c#L3481 */
    if (usb_cam_streamparm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME)
    {
        p_usb_cam_fract->numerator = 1;
        p_usb_cam_fract->denominator = V4L2_REQUESTED_FPS;

        assert(ioctl(usb_cam_fd, VIDIOC_S_PARM, &usb_cam_streamparm) == 0);
    }

    /* Print information of YUYV frame */
    printf("YUYV frame: '%dx%dp (%d/%d)'\n", usb_cam_fmt.fmt.pix.width,
                                             usb_cam_fmt.fmt.pix.height,
                                             p_usb_cam_fract->numerator,
                                             p_usb_cam_fract->denominator);

    /**********************************************
     * Step 3: Allocate buffer for the USB camera *
     **********************************************/

    /* Request and allocate a buffer for the USB camera */
    memset(&usb_cam_reqbufs, 0, sizeof(struct v4l2_requestbuffers));
    usb_cam_reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    usb_cam_reqbufs.memory = V4L2_MEMORY_MMAP;
    usb_cam_reqbufs.count = 1;

    /* Link: https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/
     *       vidioc-reqbufs.html */
    assert(ioctl(usb_cam_fd, VIDIOC_REQBUFS, &usb_cam_reqbufs) == 0);
    assert(usb_cam_reqbufs.count > 0);

    /* Get virtual address of the buffer */
    memset(&usb_cam_buf, 0, sizeof(struct v4l2_buffer));
    usb_cam_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    usb_cam_buf.memory = V4L2_MEMORY_MMAP;
    usb_cam_buf.index = 0;

    /* Link: https://www.kernel.org/doc/html/v5.0/media/uapi/v4l/
     *       vidioc-querybuf.html */
    assert(ioctl(usb_cam_fd, VIDIOC_QUERYBUF, &usb_cam_buf) == 0);

    /* Map 'usb_cam_fd' into memory */
    p_yuyv_user_virt_addr = mmap(NULL, /* Kernel chooses (page-aligned) address
                                        * at which to create mapping */
                                 usb_cam_buf.length, /* Length of the mapping */
                                 PROT_READ | PROT_WRITE, /* Recommended */
                                 MAP_SHARED,             /* Recommended */
                                 usb_cam_fd,
                                 usb_cam_buf.m.offset);

    assert(usb_cam_buf.length == YUYV_FRAME_SIZE_IN_BYTES);
    assert(p_yuyv_user_virt_addr != MAP_FAILED);

    /* Export the buffer as a dmabuf file descriptor */
    memset(&usb_cam_expbuf, 0, sizeof(struct v4l2_exportbuffer));
    usb_cam_expbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    usb_cam_expbuf.index = 0;

    /* Link: https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/
     *       vidioc-expbuf.html */
    assert(ioctl(usb_cam_fd, VIDIOC_EXPBUF, &usb_cam_expbuf) == 0);

    yuyv_dmabuf_fd = usb_cam_expbuf.fd;
    assert(yuyv_dmabuf_fd != -1);

    /****************************
     * Step 4: Start capturing  *
     ****************************/

    /* Note: For capturing applications, it is customary to first enqueue all
     * mapped buffers, then to start capturing and enter the read loop.
     * Here the application waits until a filled buffer can be dequeued, and
     * re-enqueues the buffer when the data is no longer needed.
     *
     * Link: https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/mmap.html */

    /* Enqueue the empty buffer in the driver's incoming queue.
     * Link: https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/
     *       vidioc-qbuf.html */
    memset(&usb_cam_buf, 0, sizeof(struct v4l2_buffer));
    usb_cam_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    usb_cam_buf.memory = V4L2_MEMORY_MMAP;
    usb_cam_buf.index = 0;

    assert(ioctl(usb_cam_fd, VIDIOC_QBUF, &usb_cam_buf) == 0);

    /* Start streaming I/O
     * Link: https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/
     *       vidioc-streamon.html */
    usb_cam_buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    assert(ioctl(usb_cam_fd, VIDIOC_STREAMON, &usb_cam_buf_type) == 0);

    /* Dequeue the filled buffer from the driver's outgoing process
     *
     * Note: By default, 'VIDIOC_DQBUF' blocks when no buffer is in the
     * outgoing queue. When the 'O_NONBLOCK' flag was given to the 'open()'
     * function, 'VIDIOC_DQBUF' returns immediately with an 'EAGAIN' error
     * code when no buffer is available.
     *
     * Link: https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/
     *       vidioc-qbuf.html */
    memset(&usb_cam_buf, 0, sizeof(struct v4l2_buffer));
    usb_cam_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    usb_cam_buf.memory = V4L2_MEMORY_MMAP;

    assert(ioctl(usb_cam_fd, VIDIOC_DQBUF, &usb_cam_buf) == 0);
    assert(usb_cam_buf.bytesused == YUYV_FRAME_SIZE_IN_BYTES);

    /******************************************
     * Step 5: Get YUYV data from USB camera. *
     *         Then write to a file           *
     ******************************************/

    file_write_buffer("out-yuyv-640x480-1.raw",
                      p_yuyv_user_virt_addr, YUYV_FRAME_SIZE_IN_BYTES);

    /* Stop streaming I/O
     * Link: https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/
     *       vidioc-streamon.html */
    usb_cam_buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    assert(ioctl(usb_cam_fd, VIDIOC_STREAMOFF, &usb_cam_buf_type) == 0);

    /**********************************************
     * Step 6: Set up EGL display and EGL context *
     **********************************************/

    EGLDisplay egl_display = EGL_NO_DISPLAY;
    EGLContext egl_context = EGL_NO_CONTEXT;

    EGLConfig fb_config;
    EGLint number_of_configs = 0;

    EGLint config_attribs[] =
    {
        /* Config supports creating pixel buffer surfaces */
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,

        /* Config supports creating OpenGL ES 2.0 contexts */
        EGL_CONFORMANT, EGL_OPENGL_ES2_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,

        /* RGB color buffer */
        EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER,

        /* No depth buffer */
        EGL_DEPTH_SIZE, 0,

        /* No stencil buffer */
        EGL_STENCIL_SIZE, 0,

        /* No luminance component of color buffer */
        EGL_LUMINANCE_SIZE, 0,

        /* Prefer 8-bit R, G, B, and A components */
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,

        /* Support binding of color buffers to OpenGL ES RGBA texture */
        EGL_BIND_TO_TEXTURE_RGBA, EGL_TRUE,
        EGL_NONE,
    };

    const EGLint context_attribs[] =
    {
        /* The requested major version of an OpenGL ES context */
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE,
    };

    EGLBoolean b_egl_initialize_ok = EGL_FALSE;
    EGLBoolean b_egl_makecurrent_ok = EGL_FALSE;

    /* Obtain the EGL display connection for EGL_DEFAULT_DISPLAY */
    egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    assert(egl_display != EGL_NO_DISPLAY);

    /* Initialize the EGL display connection obtained with eglGetDisplay() */
    b_egl_initialize_ok = eglInitialize(egl_display, NULL, NULL);
    assert(b_egl_initialize_ok == EGL_TRUE);

    /* Get a list of all EGL framebuffer configurations that match the
     * 'config_attribs' */
    eglChooseConfig(egl_display, config_attribs, &fb_config, 1,
                    &number_of_configs);
    assert(number_of_configs == 1);

    /* Create an EGL rendering context for the current rendering API.
     * The context can then be used to render into an EGL drawing surface */
    egl_context = eglCreateContext(egl_display, fb_config, EGL_NO_CONTEXT,
                                   context_attribs);
    assert(egl_context !=  EGL_NO_CONTEXT);

    /* Bind context to the current rendering thread and to the 'draw' and
     * 'read' surfaces */
    b_egl_makecurrent_ok = eglMakeCurrent(egl_display, EGL_NO_SURFACE,
                                          EGL_NO_SURFACE, egl_context);
    assert(b_egl_makecurrent_ok == EGL_TRUE);

    /************************************************
     * Step 7: Set up a place to render a rectangle *
     ************************************************/

    EGLCREATEIMAGEKHR eglCreateImageKHR = NULL;
    EGLDESTROYIMAGEKHR eglDestroyImageKHR = NULL;
    GLEGLIMAGETARGETTEXTURE2DOES glEGLImageTargetTexture2DOES = NULL;

    EGLint yuyv_eglimage_attribs[] =
    {
        /* The logical dimensions of YUYV buffer in pixels */
        EGL_WIDTH, YUYV_FRAME_WIDTH_IN_PIXELS,
        EGL_HEIGHT, YUYV_FRAME_HEIGHT_IN_PIXELS,

        /* Pixel format of the buffer, as specified by 'drm_fourcc.h' */
        EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_YUYV,

        /* The dmabuf file descriptor of plane 0 of the image */
        EGL_DMA_BUF_PLANE0_FD_EXT, yuyv_dmabuf_fd,

        /* The offset from the start of the dmabuf of the first sample in
         * plane 0, in bytes */
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,

        /* The number of bytes between the start of subsequent rows of samples
         * in plane 0 */
        EGL_DMA_BUF_PLANE0_PITCH_EXT, YUYV_FRAME_WIDTH_IN_PIXELS * 2/*bytes*/,

        /* Y, U, and V color range from [0, 255] */
        EGL_SAMPLE_RANGE_HINT_EXT, EGL_YUV_FULL_RANGE_EXT,

        /* The chroma samples are sub-sampled only in horizontal dimension,
         * by a factor of 2 */
        EGL_YUV_CHROMA_VERTICAL_SITING_HINT_EXT, EGL_YUV_CHROMA_SITING_0_EXT,
        EGL_YUV_CHROMA_HORIZONTAL_SITING_HINT_EXT,
                                                EGL_YUV_CHROMA_SITING_0_5_EXT,
        EGL_NONE,
    };

    EGLImageKHR yuyv_eglimage = EGL_NO_IMAGE_KHR;
    GLuint yuyv_texture_id = 0;

    GLuint yuyv_framebuffer = 0;

    /* Get address of function eglCreateImageKHR */
    eglCreateImageKHR = (EGLCREATEIMAGEKHR)
                        eglGetProcAddress("eglCreateImageKHR");
    assert(eglCreateImageKHR != NULL);

    /* Get address of function eglDestroyImageKHR */
    eglDestroyImageKHR = (EGLDESTROYIMAGEKHR)
                         eglGetProcAddress("eglDestroyImageKHR");
    assert(eglDestroyImageKHR != NULL);

    /* Get address of function glEGLImageTargetTexture2DOES */
    glEGLImageTargetTexture2DOES = (GLEGLIMAGETARGETTEXTURE2DOES)
                        eglGetProcAddress("glEGLImageTargetTexture2DOES");
    assert(glEGLImageTargetTexture2DOES != NULL);

    /* Create EGLImage from a Linux dmabuf file descriptor
     * Note: https://registry.khronos.org/EGL/extensions/EXT/
     *                       EGL_EXT_image_dma_buf_import.txt */
    yuyv_eglimage = eglCreateImageKHR(egl_display, EGL_NO_CONTEXT,
                                      EGL_LINUX_DMA_BUF_EXT,
                                      (EGLClientBuffer)NULL,
                                      yuyv_eglimage_attribs);
    assert(yuyv_eglimage != EGL_NO_IMAGE_KHR);

    /* Create and bind framebuffer */
    glGenFramebuffers(1, &yuyv_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, yuyv_framebuffer);

    /* Create external texture since GL_TEXTURE_2D does not support YUYV */
    glGenTextures(1, &yuyv_texture_id);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, yuyv_texture_id);

    /* Min filter and mag filter should be set to GL_NEAREST.
     * The output quality may be affected if these are set to GL_LINEAR */
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    /* In GL_TEXTURE_EXTERNAL_OES, only GL_CLAMP_TO_EDGE is accepted as
     * GL_TEXTURE_WRAP_S and GL_TEXTURE_WRAP_T */
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES,
                    GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES,
                    GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    /* Define texture from an existing EGLImage */
    glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, yuyv_eglimage);

    /* Attach YUYV texture to the color buffer of currently bound framebuffer */
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_EXTERNAL_OES, yuyv_texture_id, 0);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    /*******************************************
     * Step 8: Draw the rectangle on YUYV data *
     *******************************************/

    GLuint vertex_shader_object = 0;
    GLuint fragment_shader_object = 0;
    GLuint shape_rendering_prog = 0;

    /* Vertices of rectangle located in the middle of YUYV frame */
    GLfloat rec_vertices[] =
    {
        /* Positions                      Colors */
        -0.2f, -0.2f, /* Bottom-left  */  1.0f, 1.0f, 1.0f, /* White */
         0.2f, -0.2f, /* Bottom-right */  1.0f, 0.0f, 0.0f, /* Red   */
         0.2f,  0.2f, /* Top-right    */  0.0f, 1.0f, 0.0f, /* Green */
        -0.2f,  0.2f, /* Top-left     */  0.0f, 0.0f, 1.0f, /* Blue  */
    };

    /* Set of indices that refer to 'rec_vertices' */
    GLushort rec_indices[] =
    {
        0, 1, 2,
        2, 3, 0,
    };

    GLuint vbo_rec_vertices = 0;
    GLuint ibo_rec_indices = 0;

    /* Create and compile vertex shader object */
    vertex_shader_object = shader_create("shape-rendering.vs.glsl",
                                         GL_VERTEX_SHADER);
    assert(vertex_shader_object != 0);

    /* Create and compile fragment shader object */
    fragment_shader_object = shader_create("shape-rendering.fs.glsl",
                                           GL_FRAGMENT_SHADER);
    assert(fragment_shader_object != 0);

    /* Create program object and
     * link vertex shader object and fragment shader object to it */
    shape_rendering_prog = program_create(vertex_shader_object,
                                          fragment_shader_object);
    assert(shape_rendering_prog != 0);

    /* The vertex shader object and fragmented shader object are not needed
     * after creating program. So, it should be deleted */
    glDeleteShader(vertex_shader_object);
    glDeleteShader(fragment_shader_object);

    glViewport(0, 0, YUYV_FRAME_WIDTH_IN_PIXELS, YUYV_FRAME_HEIGHT_IN_PIXELS);
    glUseProgram(shape_rendering_prog);

    /* Create vertex/index buffer objects and add data to it */
    glGenBuffers(1, &vbo_rec_vertices);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_rec_vertices);
    glBufferData(GL_ARRAY_BUFFER, sizeof(rec_vertices), rec_vertices,
                 GL_STATIC_DRAW);

    glGenBuffers(1, &ibo_rec_indices);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_rec_indices);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(rec_indices), rec_indices,
                 GL_STATIC_DRAW);

    /* Enable attribute 'aPos' since it's disabled by default */
    glEnableVertexAttribArray(0);

    /* Enable attribute 'aColor' since it's disabled by default */
    glEnableVertexAttribArray(1);

    /* Show OpenGL ES how the 'rec_vertices' should be interpreted */
    glBindBuffer(GL_ARRAY_BUFFER, vbo_rec_vertices);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                          5 * sizeof(GLfloat), (void *)0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                          5 * sizeof(GLfloat), (void *)(2 * sizeof(GLfloat)));

    /* Draw the rectangle */
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_rec_indices);
    glDrawElements(GL_TRIANGLES, sizeof(rec_indices) / sizeof(GLushort),
                   GL_UNSIGNED_SHORT, 0);

    /* Wait until 'glDrawElements' finishes */
    glFinish();

    /*******************************************************
     * Step 9: Get YUYV data and rectangle embedded on it. *
     *         Then write to a file                        *
     *******************************************************/

    file_write_buffer("out-yuyv-640x480-2.raw",
                      p_yuyv_user_virt_addr, YUYV_FRAME_SIZE_IN_BYTES);

    /**********************************************
     * Step 10: Create DMA buffer for NV12 data   *
     **********************************************/

    MMNGR_ID nv12_mmngr_id = -1;
    unsigned long nv12_phy_addr = 0;
    unsigned long nv12_hard_addr = 0;
    unsigned long nv12_user_virt_addr = 0;

    int nv12_plane0_dmabuf_id = -1;
    int nv12_plane0_dmabuf_fd = -1;

    int nv12_plane1_dmabuf_id = -1;
    int nv12_plane1_dmabuf_fd = -1;

    int b_mmngr_alloc_ok = R_MM_OK;
    int b_mmngr_export_ok = R_MM_OK;

    /* Exit program if size of plane 0 of NV12 frame is not aligned to
     * page size */
    assert(page_size_is_size_aligned(NV12_PLANE0_SIZE_IN_BYTES));

    /* Calculate dmabuf size of plane 1 of NV12 frame */
    const size_t nv12_plane1_size =
                          page_size_get_aligned_size(NV12_PLANE1_SIZE_IN_BYTES);

    if (nv12_plane1_size != NV12_PLANE1_SIZE_IN_BYTES)
    {
        printf("Size of plane 1 of NV12 frame is not aligned to page size\n");
    }

    /* Allocate memory space */
    b_mmngr_alloc_ok = mmngr_alloc_in_user(&nv12_mmngr_id,
                                           NV12_PLANE0_SIZE_IN_BYTES +
                                           nv12_plane1_size,
                                           &nv12_phy_addr,
                                           &nv12_hard_addr,
                                           &nv12_user_virt_addr,
                                           MMNGR_VA_SUPPORT);
    assert(b_mmngr_alloc_ok == R_MM_OK);

    /* For plane 0 which contains Y values:
     * Allocate dmabuf's fd from the address and the address of memory space */
    b_mmngr_export_ok = mmngr_export_start_in_user(&nv12_plane0_dmabuf_id,
                                                   NV12_PLANE0_SIZE_IN_BYTES,
                                                   nv12_hard_addr,
                                                   &nv12_plane0_dmabuf_fd);
    assert(b_mmngr_export_ok == R_MM_OK);

    /* For plane 1 which contains U and V values:
     * Allocate dmabuf's fd from the address and the address of memory space */
    b_mmngr_export_ok = mmngr_export_start_in_user(&nv12_plane1_dmabuf_id,
                                                   nv12_plane1_size,
                                                   nv12_hard_addr +
                                                   NV12_PLANE0_SIZE_IN_BYTES,
                                                   &nv12_plane1_dmabuf_fd);
    assert(b_mmngr_export_ok == R_MM_OK);

    /**************************************************************************
     * Step 11: Set up a place to hold NV12 data (after conversion from YUYV) *
     **************************************************************************/

    EGLint nv12_eglimage_attribs[] =
    {
        /* The logical dimensions of NV12 buffer in pixels */
        EGL_WIDTH, NV12_FRAME_WIDTH_IN_PIXELS,
        EGL_HEIGHT, NV12_FRAME_HEIGHT_IN_PIXELS,

        /* Pixel format of the buffer, as specified by 'drm_fourcc.h' */
        EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_NV12,

        EGL_DMA_BUF_PLANE0_FD_EXT, nv12_plane0_dmabuf_fd,
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
        EGL_DMA_BUF_PLANE0_PITCH_EXT, NV12_PLANE0_WIDTH_IN_PIXELS * 1/*byte*/,

        /* These attribute-value pairs are necessary because NV12 contains 2
         * planes: plane 0 is for Y values and plane 1 is for chroma values */
        EGL_DMA_BUF_PLANE1_FD_EXT, nv12_plane1_dmabuf_fd,
        EGL_DMA_BUF_PLANE1_OFFSET_EXT, 0,
        EGL_DMA_BUF_PLANE1_PITCH_EXT, NV12_PLANE1_WIDTH_IN_PIXELS * 2/*bytes*/,

        /* Y, U, and V color range from [0, 255] */
        EGL_SAMPLE_RANGE_HINT_EXT, EGL_YUV_FULL_RANGE_EXT,

        /* The chroma plane are sub-sampled in both horizontal and vertical
         * dimensions, by a factor of 2 */
        EGL_YUV_CHROMA_VERTICAL_SITING_HINT_EXT, EGL_YUV_CHROMA_SITING_0_5_EXT,
        EGL_YUV_CHROMA_HORIZONTAL_SITING_HINT_EXT,
                                                EGL_YUV_CHROMA_SITING_0_5_EXT,
        EGL_NONE,
    };

    EGLImageKHR nv12_eglimage = EGL_NO_IMAGE_KHR;
    GLuint nv12_texture_id = 0;

    GLuint nv12_framebuffer = 0;

    /* Create EGLImage from a Linux dmabuf file descriptor */
    nv12_eglimage = eglCreateImageKHR(egl_display, EGL_NO_CONTEXT,
                                      EGL_LINUX_DMA_BUF_EXT,
                                      (EGLClientBuffer)NULL,
                                      nv12_eglimage_attribs);
    assert(nv12_eglimage != EGL_NO_IMAGE_KHR);

    /* Create and bind framebuffer */
    glGenFramebuffers(1, &nv12_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, nv12_framebuffer);

    /* Create external texture since GL_TEXTURE_2D does not support NV12 */
    glGenTextures(1, &nv12_texture_id);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, nv12_texture_id);

    /* Min filter and mag filter should be set to GL_NEAREST.
     * The output quality may be affected if these are set to GL_LINEAR */
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    /* In GL_TEXTURE_EXTERNAL_OES, only GL_CLAMP_TO_EDGE is accepted as
     * GL_TEXTURE_WRAP_S and GL_TEXTURE_WRAP_T */
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES,
                    GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES,
                    GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    /* Define texture from an existing EGLImage */
    glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, nv12_eglimage);

    /* Attach NV12 texture to the color buffer of currently bound framebuffer */
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_EXTERNAL_OES, nv12_texture_id, 0);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    /*********************************
     * Step 12: Convert YUYV to NV12 *
     *********************************/

    GLuint yuyv_to_nv12_conv_prog = 0;

    /* No zooming in/out and convert entire YUYV texture */
    GLfloat canvas_vertices[] =
    {
        /* Positions   Texture coordinates */
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
    };

    /* Set of indices that refer to position part of 'canvas_vertices' */
    GLushort canvas_indices[] =
    {
        0, 1, 2,
        2, 3, 0,
    };

    GLuint vbo_canvas_vertices = 0;
    GLuint ibo_canvas_indices = 0;
    GLint uniform_yuyv_texture = -1;

    /* Create and compile vertex shader object */
    vertex_shader_object = shader_create("yuyv-to-nv12-conv.vs.glsl",
                                         GL_VERTEX_SHADER);
    assert(vertex_shader_object != 0);

    /* Create and compile fragment shader object */
    fragment_shader_object = shader_create("yuyv-to-nv12-conv.fs.glsl",
                                           GL_FRAGMENT_SHADER);
    assert(fragment_shader_object != 0);

    /* Create program object and
     * link vertex shader object and fragment shader object to it */
    yuyv_to_nv12_conv_prog = program_create(vertex_shader_object,
                                            fragment_shader_object);
    assert(yuyv_to_nv12_conv_prog != 0);

    /* The vertex shader object and fragmented shader object are not needed
     * after creating program. So, it should be deleted */
    glDeleteShader(vertex_shader_object);
    glDeleteShader(fragment_shader_object);

    glUseProgram(yuyv_to_nv12_conv_prog);

    /* Get variable 'yuyvTexture' from fragment shader */
    uniform_yuyv_texture = glGetUniformLocation(yuyv_to_nv12_conv_prog,
                                                "yuyvTexture");
    assert(uniform_yuyv_texture != -1);

    /* Create vertex buffer objects and add data to these objects */
    glGenBuffers(1, &vbo_canvas_vertices);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_canvas_vertices);
    glBufferData(GL_ARRAY_BUFFER, sizeof(canvas_vertices), canvas_vertices,
                 GL_STATIC_DRAW);

    glGenBuffers(1, &ibo_canvas_indices);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_canvas_indices);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(canvas_indices),
                 canvas_indices, GL_STATIC_DRAW);

    /* Enable attribute 'aPos' since it's disabled by default */
    glEnableVertexAttribArray(0);

    /* Enable attribute 'aYuyvTexCoord' since it's disabled by default */
    glEnableVertexAttribArray(1);

    /* Show OpenGL ES how the 'canvas_vertices' should be interpreted */
    glBindBuffer(GL_ARRAY_BUFFER, vbo_canvas_vertices);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(GLfloat), (void*)0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));

    /* Tell OpenGL sampler 'yuyvTexture' belongs to texture unit 0 */
    glUniform1i(uniform_yuyv_texture, 0);

    /* Bind the YUYV texture to texture unit 0 */
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, yuyv_texture_id);

    /* Draw the rectangle */
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_canvas_indices);
    glDrawElements(GL_TRIANGLES, sizeof(canvas_indices) / sizeof(GLushort),
                   GL_UNSIGNED_SHORT, 0);

    /* Wait until 'glDrawElements' finishes */
    glFinish();

    /**************************************
     * Step 13: Write NV12 data to a file *
     **************************************/

    file_write_buffer("out-nv12-640x480.raw",
                      (char *)nv12_user_virt_addr, NV12_FRAME_SIZE_IN_BYTES);

    /*******************************************
     * Step 14: Encode NV12 data to H.264 data *
     *******************************************/

    shared_data_t shared_data;

    OMX_PARAM_COMPONENTROLETYPE video_enc_role;
    OMX_VIDEO_PARAM_BITRATETYPE video_bitrate_type;

    OMX_VIDEO_PORTDEFINITIONTYPE * p_video_port_def = NULL;

    OMX_BUFFERHEADERTYPE ** pp_dummy_in_bufs = NULL;
    mmngr_buf_t * p_mmngr_bufs = NULL;

    OMX_ERRORTYPE omx_error = OMX_ErrorNone;
    OMX_CALLBACKTYPE callbacks =
    {
        .EventHandler       = omx_event_handler,
        .EmptyBufferDone    = omx_empty_buffer_done,
        .FillBufferDone     = omx_fill_buffer_done
    };

    /* Initialize semaphore structures with initial value 0.
     * Note: They are to be shared between threads of this process */
    sem_init(&(shared_data.sem_event_idle), 0, 0);
    sem_init(&(shared_data.sem_event_exec), 0, 0);

    sem_init(&(shared_data.sem_h264_hdr_done), 0, 0);
    sem_init(&(shared_data.sem_h264_data_done), 0, 0);

    /* Open file for writing H.264 data */
    shared_data.p_h264_fd = fopen("out-h264-640x480.264", "w");

    /* Initialize OMX IL core */
    omx_error = OMX_Init();
    assert(omx_error == OMX_ErrorNone);

    /* Locate Renesas's H.264 encoder */
    omx_error = OMX_GetHandle(&(shared_data.video_enc_handle),
                              RENESAS_VIDEO_ENCODER_NAME,
                              (OMX_PTR)&shared_data, &callbacks);
    /* If successful, the component will be in state LOADED */
    assert(omx_error == OMX_ErrorNone);

    /* Optional: Check role of the component */
    OMX_INIT_STRUCTURE(&video_enc_role);
    omx_error = OMX_GetParameter(shared_data.video_enc_handle,
                                 OMX_IndexParamStandardComponentRole,
                                 &video_enc_role);
    assert(omx_error == OMX_ErrorNone);

    printf("Role of video encoder: %s\n", video_enc_role.cRole);

    /**********************************************
     * Step 15.1: Configure input port definition *
     **********************************************/

    /* Get input port */
    OMX_INIT_STRUCTURE(&(shared_data.in_port));
    shared_data.in_port.nPortIndex = 0; /* Index 0 is for input port */

    omx_error = OMX_GetParameter(shared_data.video_enc_handle,
                                 OMX_IndexParamPortDefinition,
                                 &(shared_data.in_port));
    assert(omx_error == OMX_ErrorNone);

    /* Configure and set new parameters to input port */
    p_video_port_def = &(shared_data.in_port.format.video);
    p_video_port_def->nFrameWidth   = OMX_INPUT_FRAME_WIDTH;
    p_video_port_def->nFrameHeight  = OMX_INPUT_FRAME_HEIGHT;
    p_video_port_def->xFramerate    = OMX_INPUT_FRAME_RATE << 16; /*Q16 format*/
    p_video_port_def->nStride       = OMX_INPUT_STRIDE;
    p_video_port_def->nSliceHeight  = OMX_INPUT_SLICE_HEIGHT;
    p_video_port_def->eColorFormat  = OMX_INPUT_COLOR_FORMAT;

    omx_error = OMX_SetParameter(shared_data.video_enc_handle,
                                 OMX_IndexParamPortDefinition,
                                 &(shared_data.in_port));
    assert(omx_error == OMX_ErrorNone);

    /* Update input port */
    omx_error = OMX_GetParameter(shared_data.video_enc_handle,
                                 OMX_IndexParamPortDefinition,
                                 &(shared_data.in_port));
    assert(omx_error == OMX_ErrorNone);

    /***********************************************
     * Step 15.2: Configure output port definition *
     ***********************************************/

    /* Get output port */
    OMX_INIT_STRUCTURE(&(shared_data.out_port));
    shared_data.out_port.nPortIndex = 1; /* Index 1 is for output port */

    omx_error = OMX_GetParameter(shared_data.video_enc_handle,
                                 OMX_IndexParamPortDefinition,
                                 &(shared_data.out_port));
    assert(omx_error == OMX_ErrorNone);

    /* Configure and set new parameters to output port */
    p_video_port_def = &(shared_data.out_port.format.video);
    p_video_port_def->nBitrate = OMX_OUTPUT_BIT_RATE;
    p_video_port_def->eCompressionFormat = OMX_OUTPUT_COMPRESSION_FORMAT;

    omx_error = OMX_SetParameter(shared_data.video_enc_handle,
                                 OMX_IndexParamPortDefinition,
                                 &(shared_data.out_port));
    assert(omx_error == OMX_ErrorNone);

    /* Update output port */
    omx_error = OMX_GetParameter(shared_data.video_enc_handle,
                                 OMX_IndexParamPortDefinition,
                                 &(shared_data.out_port));
    assert(omx_error == OMX_ErrorNone);

    /****************************************************
     * Step 15.3: Set constant bit rate for output port *
     ****************************************************/

    OMX_INIT_STRUCTURE(&video_bitrate_type);
    video_bitrate_type.nPortIndex = shared_data.out_port.nPortIndex;

    omx_error = OMX_GetParameter(shared_data.video_enc_handle,
                                 OMX_IndexParamVideoBitrate,
                                 &video_bitrate_type);
    assert(omx_error == OMX_ErrorNone);

    /* Configure bit rate control types.
     *
     * Note: We do not need to set 'nTargetBitrate' since we already
     * set 'nBitrate' of output port */
    video_bitrate_type.eControlRate = OMX_Video_ControlRateConstant;

    omx_error = OMX_SetParameter(shared_data.video_enc_handle,
                                 OMX_IndexParamVideoBitrate,
                                 &video_bitrate_type);
    assert(omx_error == OMX_ErrorNone);

    /*********************************************************
     * Step 15.4: Use buffer 'nv12_hard_addr' for input port *
     *********************************************************/

    /* Transition into state IDLE.
     * This state indicates that the component has all needed static resources,
     * but it's not processing data */
    omx_error = OMX_SendCommand(shared_data.video_enc_handle,
                                OMX_CommandStateSet, OMX_StateIdle, NULL);
    assert(omx_error == OMX_ErrorNone);

    /* See section 2.2.9 in document 'R01USxxxxEJxxxx_cmn_v1.0.pdf'
     * and section 4.1.1 in document 'R01USxxxxEJxxxx_vecmn_v1.0.pdf' */
    omx_error = OMX_UseBuffer(shared_data.video_enc_handle,
                              &(shared_data.p_in_buf),
                              shared_data.in_port.nPortIndex,
                              NULL,
                              shared_data.in_port.nBufferSize,
                              (OMX_U8 *)nv12_hard_addr);

    /* See 'Table 6-3' in document 'R01USxxxxEJxxxx_vecmn_v1.0.pdf' */
    assert(shared_data.in_port.nBufferSize == NV12_FRAME_SIZE_IN_BYTES);
    assert(omx_error == OMX_ErrorNone);

    /* Create dummy buffers for input port */
    const OMX_U32 in_bufs_size = shared_data.in_port.nBufferCountActual;
    if (in_bufs_size > 1)
    {
        /* The first buffer is already taken care of by 'OMX_UseBuffer' */
        pp_dummy_in_bufs = (OMX_BUFFERHEADERTYPE **)malloc((in_bufs_size - 1) *
                                                sizeof(OMX_BUFFERHEADERTYPE *));

        p_mmngr_bufs = (mmngr_buf_t *)malloc((in_bufs_size - 1) *
                                             sizeof(mmngr_buf_t));

        for (int i = 0; i < in_bufs_size - 1; i++)
        {
            b_mmngr_alloc_ok = mmngr_alloc_in_user(
                                   &(p_mmngr_bufs[i].mmngr_id),
                                   shared_data.in_port.nBufferSize,
                                   &(p_mmngr_bufs[i].phy_addr),
                                   &(p_mmngr_bufs[i].hard_addr),
                                   &(p_mmngr_bufs[i].user_virt_addr),
                                   MMNGR_VA_SUPPORT);
            assert(b_mmngr_alloc_ok == R_MM_OK);

            omx_error = OMX_UseBuffer(shared_data.video_enc_handle,
                                      &(pp_dummy_in_bufs[i]),
                                      shared_data.in_port.nPortIndex,
                                      NULL,
                                      shared_data.in_port.nBufferSize,
                                      (OMX_U8 *)p_mmngr_bufs[i].hard_addr);
            assert(omx_error == OMX_ErrorNone);
        }
    }

    /***********************************************
     * Step 15.5: Allocate buffers for output port *
     ***********************************************/

    const OMX_U32 out_bufs_size = shared_data.out_port.nBufferCountActual;
    shared_data.pp_out_bufs = (OMX_BUFFERHEADERTYPE **)malloc(out_bufs_size *
                                                sizeof(OMX_BUFFERHEADERTYPE *));

    for (int i = 0; i < out_bufs_size; i++)
    {
        /* See section 2.2.10 in document 'R01USxxxxEJxxxx_cmn_v1.0.pdf'
         * and 'Table 6-3' in document 'R01USxxxxEJxxxx_vecmn_v1.0.pdf' */
        omx_error = OMX_AllocateBuffer(shared_data.video_enc_handle,
                                       &(shared_data.pp_out_bufs[i]),
                                       shared_data.out_port.nPortIndex,
                                       NULL,
                                       shared_data.out_port.nBufferSize);
        assert(omx_error == OMX_ErrorNone);
    }

    /* We must allocate all necessary buffers for input port before the
     * component is in state IDLE.
     *
     * The number of buffers is defined in
     * 'OMX_PARAM_PORTDEFINITIONTYPE::nBufferCountActual'.
     * However, it can be set to a lower value, but not less than
     * 'OMX_PARAM_PORTDEFINITIONTYPE::nBufferCountMin'.
     *
     * But, in most case, 'nBufferCountMin' is often greater than 1 and
     * equal to 'nBufferCountActual' */
    sem_wait(&(shared_data.sem_event_idle));

    /* Transition into state EXECUTING.
     *
     * This state indicates that the component is pending reception of buffers
     * to process data and will make required callbacks
     * (see section 3.1.2.9 'OMX_CALLBACKTYPE' in OMX IL specification 1.1.2) */
    omx_error = OMX_SendCommand(shared_data.video_enc_handle,
                                OMX_CommandStateSet, OMX_StateExecuting, NULL);
    assert(omx_error == OMX_ErrorNone);
    sem_wait(&(shared_data.sem_event_exec));

    /*******************************************
     * Step 15.6: Send NV12 data to input port *
     *******************************************/

    /* Send buffer 'p_in_buf' to the input port of the component.
     * If the buffer contains data, 'p_in_buf->nFilledLen' will not be zero.
     *
     * TODO: Buffer 'p_in_buf' should have a flag so that we can know
     * whether it's okay to pass it to 'OMX_EmptyThisBuffer'
     * (see section 2.2.12 in document 'R01USxxxxEJxxxx_cmn_v1.0.pdf')
     *
     * Note: 'p_in_buf->nFilledLen' is set to 0 when callback
     * 'OMX_CALLBACKTYPE::EmptyBufferDone' is returned */
    (shared_data.p_in_buf)->nFilledLen = shared_data.in_port.nBufferSize;

    omx_error = OMX_EmptyThisBuffer(shared_data.video_enc_handle,
                                    shared_data.p_in_buf);
    assert(omx_error == OMX_ErrorNone);

    /**************************************************
     * Step 15.7: Receive H.264 data from output port *
     **************************************************/

    assert(shared_data.out_port.nBufferCountActual > 1);

    /* Send buffer 'pp_out_bufs[0]' to the output port of the component.
     * It will contain SPS NAL and PPS NAL.
     *
     * If 'pp_out_bufs[0]->nOutputPortIndex' is not a valid output port,
     * the component returns 'OMX_ErrorBadPortIndex'.
     *
     * TODO: Each buffer in 'pp_out_bufs' should have a flag so that we can
     * know whether it's okay to pass these to 'OMX_FillThisBuffer'
     * (see section 2.2.13 in document 'R01USxxxxEJxxxx_cmn_v1.0.pdf') */

    omx_error = OMX_FillThisBuffer(shared_data.video_enc_handle,
                                   shared_data.pp_out_bufs[0]);
    assert(omx_error == OMX_ErrorNone);

    /* Send buffer 'pp_out_bufs[1]' to the output port of the component.
     * It will contain Data NAL */
    omx_error = OMX_FillThisBuffer(shared_data.video_enc_handle,
                                   shared_data.pp_out_bufs[1]);
    assert(omx_error == OMX_ErrorNone);

    /* Wait until the component fills data to 'pp_out_bufs[0]' and
     * 'pp_out_bufs[1]' (see callback 'FillBufferDone').
     *
     * TODOs: Should create 2 threads, one is for sending NV12 frames to
     * input port and one is for requesting H.264 data from output port.
     *
     * Warning: We should pay attention for errors listed in 'Table 7-1'
     * (document 'R01USxxxxEJxxxx_vecmn_v1.0.pdf') when handling data */
    sem_wait(&(shared_data.sem_h264_hdr_done));
    sem_wait(&(shared_data.sem_h264_data_done));

    /************************************
     * Step 16: Cleanup OMX's resources *
     ************************************/

    /* Close file which stores H.264 data */
    fclose(shared_data.p_h264_fd);

    /* Transition into state IDLE */
    omx_error = OMX_SendCommand(shared_data.video_enc_handle,
                                OMX_CommandStateSet, OMX_StateIdle, NULL);
    assert(omx_error == OMX_ErrorNone);
    sem_wait(&(shared_data.sem_event_idle));

    /* Transition into state LOADED */
    omx_error = OMX_SendCommand(shared_data.video_enc_handle,
                                OMX_CommandStateSet, OMX_StateLoaded, NULL);
    assert(omx_error == OMX_ErrorNone);

    /* Release buffers and buffer headers from the component
     *
     * The component shall free only the buffer headers if it allocated only
     * the buffer headers ('OMX_UseBuffer').
     *
     * The component shall free both the buffers and the buffer headers
     * if it allocated both the buffers and buffer headers
     * ('OMX_AllocateBuffer') */
    omx_error = OMX_FreeBuffer(shared_data.video_enc_handle,
                               shared_data.in_port.nPortIndex,
                               shared_data.p_in_buf);
    assert(omx_error == OMX_ErrorNone);

    if (shared_data.in_port.nBufferCountActual > 1)
    {
        for (int i = 0; i < shared_data.in_port.nBufferCountActual - 1; i++)
        {
            mmngr_free_in_user(p_mmngr_bufs[i].mmngr_id);

            omx_error = OMX_FreeBuffer(shared_data.video_enc_handle,
                                       shared_data.in_port.nPortIndex,
                                       pp_dummy_in_bufs[i]);
            assert(omx_error == OMX_ErrorNone);
        }

        free((void *)p_mmngr_bufs);
        free((void *)pp_dummy_in_bufs);
    }

    for (int i = 0; i < shared_data.out_port.nBufferCountActual; i++)
    {
        omx_error = OMX_FreeBuffer(shared_data.video_enc_handle,
                                   shared_data.out_port.nPortIndex,
                                   shared_data.pp_out_bufs[i]);
        assert(omx_error == OMX_ErrorNone);
    }
    free((void*)(shared_data.pp_out_bufs));

    /* TODO: Should have a semaphore for state LOADED */
    omx_wait_state(shared_data.video_enc_handle, OMX_StateLoaded);

    /* Free the component's handle */
    omx_error = OMX_FreeHandle(shared_data.video_enc_handle);
    assert(omx_error == OMX_ErrorNone);

    /* De-initialize OMX IL core */
    omx_error = OMX_Deinit();
    assert(omx_error == OMX_ErrorNone);

    /* Destroy semaphore structures */
    sem_destroy(&(shared_data.sem_event_idle));
    sem_destroy(&(shared_data.sem_event_exec));

    sem_destroy(&(shared_data.sem_h264_hdr_done));
    sem_destroy(&(shared_data.sem_h264_data_done));

    /*******************************************
     * Step 17: Cleanup OpenGL ES's resources  *
     *******************************************/

    /* Cleanup vertex buffer objects */
    glDeleteBuffers(1, &vbo_canvas_vertices);
    glDeleteBuffers(1, &ibo_canvas_indices);

    /* Cleanup program object */
    glDeleteProgram(yuyv_to_nv12_conv_prog);

    /* Cleanup framebuffer and texture of NV12 */
    eglDestroyImageKHR(egl_display, nv12_eglimage);
    glDeleteTextures(1, &nv12_texture_id);
    glDeleteFramebuffers(1, &nv12_framebuffer);

    /* Cleanup mmngr's id and dmabuf's id of plane 1 of NV12 */
    mmngr_export_end_in_user(nv12_plane1_dmabuf_id);

    /* Cleanup mmngr's id and dmabuf's id of plane 0 of NV12 */
    mmngr_export_end_in_user(nv12_plane0_dmabuf_id);

    /* Cleanup memory space of NV12 */
    mmngr_free_in_user(nv12_mmngr_id);

    /* Cleanup vertex buffer objects */
    glDeleteBuffers(1, &vbo_rec_vertices);
    glDeleteBuffers(1, &ibo_rec_indices);

    /* Cleanup program object */
    glDeleteProgram(shape_rendering_prog);

    /* Cleanup framebuffer and texture of YUYV */
    eglDestroyImageKHR(egl_display, yuyv_eglimage);
    glDeleteTextures(1, &yuyv_texture_id);
    glDeleteFramebuffers(1, &yuyv_framebuffer);

    /* Cleanup EGL display, surface, and context */
    eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(egl_display, egl_context);
    eglTerminate(egl_display);

    egl_display = EGL_NO_DISPLAY;
    egl_context = EGL_NO_CONTEXT;

    /*************************************
     * Step 18: Cleanup V4L2's resources *
     *************************************/

    /* Close dmabuf of YUYV buffer.
     *
     * It is recommended to close a dmabuf file when it is no longer used
     * to allow the associated memory to be reclaimed.
     *
     * Link: https://www.kernel.org/doc/html/v5.0/media/uapi/v4l/
     *       vidioc-expbuf.html */
    close(yuyv_dmabuf_fd);

    /* Unmap pages of memory */
    munmap(p_yuyv_user_virt_addr, YUYV_FRAME_SIZE_IN_BYTES);

    /* Close the USB camera */
    close(usb_cam_fd);

    return 0;
}

/* Function definitions */

size_t page_size_get_aligned_size(size_t size)
{
    /* Get page size */
    const long page_size = sysconf(_SC_PAGESIZE);

    return (size_t)(ceilf((1.0f * size) / page_size) * page_size);
}

bool page_size_is_size_aligned(size_t size)
{
    return (page_size_get_aligned_size(size) == size) ? true : false;
}

char * file_read_str(const char * p_filename)
{
    long file_size = -1;

    /* Open file */
    FILE * p_fd = fopen(p_filename, "r");
    if (p_fd == NULL)
    {
        return NULL;
    }

    /* Get file's size */
    fseek(p_fd, 0, SEEK_END);
    file_size = ftell(p_fd);
    rewind(p_fd);

    /* Prepare buffer to store file's content */
    char * p_file_content = (char *)malloc(file_size + 1);
    memset(p_file_content, 0, file_size + 1);

    /* Read file's content and put it into buffer */
    fread(p_file_content, 1, file_size, p_fd);
    p_file_content[file_size] = '\0';

    /* Close file */
    fclose(p_fd);

    return p_file_content;
}

void file_write_buffer(const char * p_filename,
                       const char * p_buffer, size_t size)
{
    /* Open file */
    FILE * p_fd = fopen(p_filename, "w");
    if (p_fd != NULL)
    {
        /* Write content to the file */
        fwrite(p_buffer, sizeof(char), size, p_fd);

        /* Close file */
        fclose(p_fd);
    }
}

GLuint shader_create(const char * p_filename, GLenum type)
{
    GLint b_compile_ok = GL_FALSE;
    
    GLint info_log_length = 0;
    char * p_info_log = NULL;

    const GLchar * p_shader_source = file_read_str(p_filename);
    if (p_shader_source == NULL)
    {
        printf("Error opening file '%s'\n", p_filename);
        return 0;
    }

    /* Create an empty shader object */
    GLuint shader_object = glCreateShader(type);
    if (shader_object == 0)
    {
        printf("Function glCreateShader() failed\n");
        return 0;
    }

    /* Copy source code into the shader object */
    glShaderSource(shader_object, 1, &p_shader_source, NULL);
    free((void *)p_shader_source);
    
    /* Compile the source code strings that was stored in the shader object */
    glCompileShader(shader_object);

    /* Get status of the last compile operation on the shader object */
    glGetShaderiv(shader_object, GL_COMPILE_STATUS, &b_compile_ok);
    if (b_compile_ok == GL_FALSE)
    {
        glGetShaderiv(shader_object, GL_INFO_LOG_LENGTH, &info_log_length);
        p_info_log = (char *)malloc(info_log_length);

        /* Get the compile error of the shader object */
        glGetShaderInfoLog(shader_object, info_log_length, NULL, p_info_log);
        printf("Shader compilation error: '%s'\n", p_info_log);
        free((void *)p_info_log);

        glDeleteShader(shader_object);
        return 0;
    }

  return shader_object;
}

GLuint program_create(GLuint vertex_shader_object,
                      GLuint fragment_shader_object)
{
    GLuint program;

    GLint b_link_ok = GL_FALSE;

    GLint info_log_length = 0;
    char * p_info_log = NULL;

    /* Create an empty program object */
    program = glCreateProgram();
    if (program == 0)
    {
        printf("Function glCreateProgram() failed\n");
        return 0;
    }

    /* Attach the shader object to the program object */
    glAttachShader(program, vertex_shader_object);
    glAttachShader(program, fragment_shader_object);

    /* Link the program object */
    glLinkProgram(program);

    /* Get status of the last link operation on program object */
    glGetProgramiv(program, GL_LINK_STATUS, &b_link_ok);
    if (b_link_ok == GL_FALSE)
    {
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_log_length);
        p_info_log = (char *)malloc(info_log_length);

        /* Get the link error of the program object */
        glGetProgramInfoLog(program, info_log_length, NULL, p_info_log);
        printf("Program linking error: '%s'\n", p_info_log);
        free((void *)p_info_log);

        glDeleteProgram(program);
        glDeleteShader(vertex_shader_object);
        glDeleteShader(fragment_shader_object);

        return 0;
    }

    return program;
}

OMX_ERRORTYPE omx_event_handler(OMX_HANDLETYPE hComponent, OMX_PTR pAppData,
                                OMX_EVENTTYPE eEvent, OMX_U32 nData1,
                                OMX_U32 nData2, OMX_PTR pEventData)
{
    shared_data_t * p_shared_data = (shared_data_t *)pAppData;
    assert(p_shared_data != NULL);

    switch (eEvent)
    {
        case OMX_EventCmdComplete:
        {
            if (nData1 != OMX_CommandStateSet)
            {
                /* Exit from top switch statement */
                break;
            }

            switch (nData2)
            {
                case OMX_StateInvalid:
                {
                    printf("OMX_StateInvalid\n");
                }
                break;

                case OMX_StateLoaded:
                {
                    printf("OMX_StateLoaded\n");
                }
                break;

                case OMX_StateIdle:
                {
                    printf("OMX_StateIdle\n");
                    sem_post(&(p_shared_data->sem_event_idle));
                }
                break;

                case OMX_StateExecuting:
                {
                    printf("OMX_StateExecuting\n");
                    sem_post(&(p_shared_data->sem_event_exec));
                }
                break;

                case OMX_StatePause:
                {
                    printf("OMX_StatePause\n");
                }
                break;

                case OMX_StateWaitForResources:
                {
                    printf("OMX_StateWaitForResources\n");
                }
                break;

                default:
                {
                    /* Intentionally left blank */
                }
                break;
            }
        }
        break;

        default:
        {
            /* Intentionally left blank */
        }
        break;
    }
    return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_empty_buffer_done(OMX_HANDLETYPE hComponent, OMX_PTR pAppData,
                                    OMX_BUFFERHEADERTYPE* pBuffer)
{
    printf("EmptyBufferDone is called\n");

    /* TODO: Should enable the flag to allow 'pBuffer' to be used
     * (see step 11.6) */

    return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_fill_buffer_done(OMX_HANDLETYPE hComponent, OMX_PTR pAppData,
                                   OMX_BUFFERHEADERTYPE* pBuffer)
{
    shared_data_t * p_shared_data = (shared_data_t *)pAppData;
    assert(p_shared_data != NULL);

    printf("FillBufferDone is called\n");

    /* TODOs:
     *   - Should enable the flag to allow 'pBuffer' to be used (see step 11.7).
     *
     *   - This is a blocking call so the application should not attempt to
     *     refill the buffers during this call, but should queue them and
     *     refill them in another thread.
     *
     *     For trial, we just write H.264 data to a file */
    fwrite((char *)pBuffer->pBuffer, sizeof(char),
           pBuffer->nFilledLen, p_shared_data->p_h264_fd);

    if (p_shared_data->pp_out_bufs[0] == pBuffer)
    {
        sem_post(&(p_shared_data->sem_h264_hdr_done));
    }
    else if (p_shared_data->pp_out_bufs[1] == pBuffer)
    {
        sem_post(&(p_shared_data->sem_h264_data_done));
    }

    return OMX_ErrorNone;
}

OMX_BOOL omx_wait_state(OMX_HANDLETYPE handle, OMX_STATETYPE state)
{
    OMX_BOOL b_is_successful = OMX_TRUE;

    OMX_ERRORTYPE omx_error = OMX_ErrorNone;
    OMX_STATETYPE omx_cur_state = OMX_StateInvalid;

    do
    {
        omx_error = OMX_GetState(handle, &omx_cur_state);
        if (omx_error != OMX_ErrorNone)
        {
            b_is_successful = OMX_FALSE;
            break;
        }
    }
    while (omx_cur_state != state);

    return b_is_successful;
}
