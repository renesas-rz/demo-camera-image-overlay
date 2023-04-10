#include <math.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>

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

#include <linux/videodev2.h>

/******************************************************************************
 *                              MACRO VARIABLES                               *
 ******************************************************************************/

#define FRAME_WIDTH_IN_PIXELS  640

#define FRAME_HEIGHT_IN_PIXELS 480

#define YUYV_FRAME_WIDTH_IN_BYTES (FRAME_WIDTH_IN_PIXELS * 2)

#define YUYV_FRAME_SIZE_IN_BYTES (FRAME_WIDTH_IN_PIXELS  * \
                                  FRAME_HEIGHT_IN_PIXELS * 2)

#define NV12_FRAME_SIZE_IN_BYTES (FRAME_WIDTH_IN_PIXELS  * \
                                  FRAME_HEIGHT_IN_PIXELS * 1.5f)

#define FRAMERATE 30 /* FPS */

/********************************** FOR V4L2 **********************************/

/* The sample app is tested OK with:
 *   - Logitech C270 HD Webcam.
 *   - Logitech C920 HD Pro Webcam.
 *   - Logitech C930e Business Webcam.
 *   - Logitech BRIO Ultra HD Pro Business Webcam */
#define USB_CAMERA_FD "/dev/video0"

/* The number of buffers to be allocated for the camera */
#define YUYV_BUFFER_COUNT 5

/********************************** FOR OMX ***********************************/

/* The component name for H.264 encoder media component */
#define RENESAS_VIDEO_ENCODER_NAME "OMX.RENESAS.VIDEO.ENCODER.H264"

/* The number of buffers to be allocated for input port of media component */
#define NV12_BUFFER_COUNT 2

/* The number of buffers to be allocated for output port of media component */
#define H264_BUFFER_COUNT 2

/* The bitrate is related to the quality of output file and compression level
 * of video encoder. For example:
 *   - With 1 Mbit/s, the encoder produces ~1.2 MB of data for 10-second video.
 *   - With 5 Mbit/s, the encoder produces ~6 MB of data for 10-second video
 *                                          and the quality should be better */
#define H264_BITRATE 5000000 /* 5 Mbit/s */

#define H264_FILE_NAME "out-h264-640x480.264"

/* Introduction to:
 *   OMX_PARAM_PORTDEFINITIONTYPE::format::video::nFrameWidth
 *   (OMX_VIDEO_PORTDEFINITIONTYPE::nFrameWidth)
 *
 * According to OMX IL specification 1.1.2, 'nFrameWidth' is the width of
 * the data in pixels.
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


/* Introduction to:
 *   OMX_PARAM_PORTDEFINITIONTYPE::format::video::nFrameHeight
 *   (OMX_VIDEO_PORTDEFINITIONTYPE::nFrameHeight)
 *
 * According to OMX IL specification 1.1.2, 'nFrameHeight' is the height of
 * the data in pixels.
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


/* Introduction to:
 *   OMX_PARAM_PORTDEFINITIONTYPE::format::video::xFramerate
 *   (OMX_VIDEO_PORTDEFINITIONTYPE::xFramerate)
 *
 * According to OMX IL specification 1.1.2, 'xFramerate' is the
 * framerate whose unit is frames per second. The value is represented
 * in Q16 format and used on the port which handles uncompressed data.
 *
 * Note: For the list of supported 'xFramerate' values, please refer to
 * 'Table 6-5' in document 'R01USxxxxEJxxxx_h264e_v1.0.pdf' */


/* Introduction to:
 *   OMX_PARAM_PORTDEFINITIONTYPE::nBufferCountActual
 *
 * According to OMX IL specification 1.1.2, 'nBufferCountActual' represents
 * the number of buffers that are required on these ports before they are
 * populated, as indicated by 'OMX_PARAM_PORTDEFINITIONTYPE::bPopulated'.
 *
 * The component shall set a default value no less than 'nBufferCountMin'
 * for this field */


/* Introduction to:
 *   OMX_PARAM_PORTDEFINITIONTYPE::format::video::nStride
 *   (OMX_VIDEO_PORTDEFINITIONTYPE::nStride)
 *
 * Basically, 'nStride' is the sum of 'nFrameWidth' and extra padding pixels
 * at the end of each row of a frame.
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
 * Notes:
 *  1. 'nStride' plays an important role in the layout of buffer data
 *     (of the input port) which can be seen in 'Figure 6-6' of document
 *     'R01USxxxxEJxxxx_vecmn_v1.0.pdf'.
 *  2. The sample app uses macro function 'OMX_STRIDE' to calculate 'nStride'
 *     from 'nFrameWidth'.
 *
 * Warning: Rule (*) is not confirmed in encoding process */


/* Introduction to:
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
 * Notes:
 *   1. 'nSliceHeight' plays an important role in the layout of buffer data
 *      (of the input port) which can be seen in 'Figure 6-6' of document
 *      'R01USxxxxEJxxxx_vecmn_v1.0.pdf'.
 *   2. The sample app uses macro function 'OMX_SLICE_HEIGHT' to calculate
 *      'nSliceHeight' from 'nFrameHeight'.
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
 * Warning: Rule (*) is not confirmed in encoding process */


/* Introduction to:
 *   OMX_PARAM_PORTDEFINITIONTYPE::format::video::eColorFormat
 *   (OMX_VIDEO_PORTDEFINITIONTYPE::eColorFormat)
 *
 * According to OMX IL specification 1.1.2, 'eColorFormat' is the
 * color format of the data of the input port.
 *
 * Note: On G2L, 'eColorFormat' will always be
 * 'OMX_COLOR_FormatYUV420SemiPlanar' which represents NV12 format */


/* Introduction to:
 *   OMX_VIDEO_PARAM_BITRATETYPE::nTargetBitrate
 *
 * According to OMX IL specification 1.1.2, 'nTargetBitrate' is the bitrate in
 * bits per second of the frame to be used on the port which handles compressed
 * data.
 *
 * 'nTargetBitrate' is the same as
 * 'OMX_PARAM_PORTDEFINITIONTYPE::format::video::nBitrate'.
 * When either is updated, the other is updated with the same value
 *
 * Note: Section 6.7.14 in document 'R01USxxxxEJxxxx_h264e_v1.0.pdf'
 * shows valid settings of the bitrate for video encoder */


/* Introduction to:
 *   OMX_PARAM_PORTDEFINITIONTYPE::format::video::eCompressionFormat
 *   (OMX_VIDEO_PORTDEFINITIONTYPE::eCompressionFormat)
 *
 * According to document 'R01USxxxxEJxxxx_h264e_v1.0.pdf',
 * 'eCompressionFormat' only accepts value 'OMX_VIDEO_CodingAVC' */

/******************************************************************************
 *                              FUNCTION MACROS                               *
 ******************************************************************************/

/* Return a value not less than 'VAL' and divisible by 'RND'
 * (based on: https://github.com/Xilinx/vcu-omx-il/blob/master/exe_omx/encoder).
 *
 * Examples:
 *   - ROUND_UP(359, 2) -> 360.
 *   - ROUND_UP(480, 2) -> 480.
 *
 *   - ROUND_UP(360, 32)  ->  384.
 *   - ROUND_UP(640, 32)  ->  640.
 *   - ROUND_UP(720, 32)  ->  736.
 *   - ROUND_UP(1280, 32) -> 1280.
 *   - ROUND_UP(1920, 32) -> 1920 */
#define ROUND_UP(VAL, RND) (((VAL) + (RND) - 1) & (~((RND) - 1)))

/* The macro is used to populate 'nSize' and 'nVersion' fields of 'P_STRUCT'
 * before passing it to one of the below functions:
 *   - OMX_GetConfig
 *   - OMX_SetConfig
 *   - OMX_GetParameter
 *   - OMX_SetParameter
 */
#define OMX_INIT_STRUCTURE(P_STRUCT)                                \
{                                                                   \
    memset((P_STRUCT), 0, sizeof(*(P_STRUCT)));                     \
                                                                    \
    (P_STRUCT)->nSize = sizeof(*(P_STRUCT));                        \
                                                                    \
    (P_STRUCT)->nVersion.s.nVersionMajor = OMX_VERSION_MAJOR;       \
    (P_STRUCT)->nVersion.s.nVersionMinor = OMX_VERSION_MINOR;       \
    (P_STRUCT)->nVersion.s.nRevision     = OMX_VERSION_REVISION;    \
    (P_STRUCT)->nVersion.s.nStep         = OMX_VERSION_STEP;        \
}

/* Get stride from frame width */
#define OMX_STRIDE(WIDTH) ROUND_UP(WIDTH, 32)

/* Get slice height from frame height */
#define OMX_SLICE_HEIGHT(HEIGHT) ROUND_UP(HEIGHT, 2)

/******************************************************************************
 *                        EGL AND OPENGL ES EXTENSIONS                        *
 ******************************************************************************/

typedef EGLImageKHR (*EGLCREATEIMAGEKHR) (EGLDisplay dpy, EGLContext ctx,
                                          EGLenum target,
                                          EGLClientBuffer buffer,
                                          EGLint * p_attr_list);

typedef EGLBoolean (*EGLDESTROYIMAGEKHR) (EGLDisplay dpy, EGLImageKHR image);

typedef void (*GLEGLIMAGETARGETTEXTURE2DOES) (GLenum target,
                                              GLeglImageOES image);

EGLCREATEIMAGEKHR eglCreateImageKHR;
EGLDESTROYIMAGEKHR eglDestroyImageKHR;
GLEGLIMAGETARGETTEXTURE2DOES glEGLImageTargetTexture2DOES;

/******************************************************************************
 *                           STRUCTURE DEFINITIONS                            *
 ******************************************************************************/

/********************************** FOR V4L2 **********************************/

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

/************************ FOR QUEUE (NOT THREAD-SAFE) *************************/

/* Circular queue data structure.
 *
 * https://www.programiz.com/dsa/queue
 * https://www.programiz.com/dsa/circular-queue */
typedef struct
{
    /* An array which contains 'elm_cnt' elements of 'elm_size' bytes each */
    void * p_array;

    uint32_t elm_cnt;
    uint32_t elm_size;

    /* An index which tracks the first element in queue */
    int front_idx;

    /* An index which tracks the last element in queue */
    int rear_idx;

} queue_t;

/********************************** FOR OMX ***********************************/

/* This structure is shared between OMX's callbacks */
typedef struct
{
    queue_t * p_in_queue;
    queue_t * p_out_queue;

    pthread_mutex_t * p_mut_in;
    pthread_mutex_t * p_mut_out;

    pthread_cond_t * p_cond_in_available;
    pthread_cond_t * p_cond_out_available;

} omx_data_t;

/******************************* FOR OPENGL ES ********************************/

typedef struct
{
    /* For drawing rectangle */
    GLuint shape_prog;
    GLuint vbo_rec_vertices;
    GLuint ibo_rec_indices;

    /* For YUYV-to-NV12 conversion */
    GLuint conv_prog;
    GLuint vbo_canvas_vertices;
    GLuint ibo_canvas_indices;
    GLint uniform_yuyv_texture;

} gl_resources_t;

/********************************* FOR MMNGR **********************************/

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

/******************************** FOR THREADS *********************************/

/* This structure is for input thread */
typedef struct
{
    /* USB camera */
    int cam_fd;

    /* YUYV buffers */
    v4l2_dmabuf_exp_t * p_yuyv_bufs;

    /* NV12 buffers */
    mmngr_buf_t * p_nv12_bufs;

    /* Handle of media component */
    OMX_HANDLETYPE handle;

    /* Buffers for input port */
    OMX_BUFFERHEADERTYPE ** pp_bufs;

    /* The queue contains some buffers in 'pp_bufs' ready to be overlaid and
     * sent to input port */
    queue_t * p_queue;

    /* The mutex structure is used to synchronize 'wait' and 'signal' events
     * of 'p_cond_available' */
    pthread_mutex_t * p_mutex;

    /* When signaled, the condition variable confirms there is a possible
     * buffer in 'p_queue' */
    pthread_cond_t * p_cond_available;

} in_data_t;

/* This structure is for output thread */
typedef struct
{
    /* Handle of media component */
    OMX_HANDLETYPE handle;

    /* This queue contains some buffers received from output port and ready
     * to be written to output file */
    queue_t * p_queue;

    /* The mutex structure is used to synchronize 'wait' and 'signal' events
     * of 'p_cond_available' */
    pthread_mutex_t * p_mutex;

    /* When signaled, the condition variable confirms there is a possible
     * buffer in 'p_queue' */
    pthread_cond_t * p_cond_available;

} out_data_t;

/******************************************************************************
 *                              GLOBAL VARIABLES                              *
 ******************************************************************************/

/* 1: Interrupt signal */
volatile sig_atomic_t g_int_signal = 0;

/******************************************************************************
 *                           FUNCTION DECLARATIONS                            *
 ******************************************************************************/

/**************************** FOR SIGNAL HANDLING *****************************/

void sigint_handler(int signum, siginfo_t * p_info, void * ptr);

/********************************** FOR V4L2 **********************************/

/* This function opens V4L2 device.
 * Return non-negative value if successful or -1 if error */
int v4l2_open_dev(const char * p_name);

/* This function checks the below conditions:
 *   - The device should support the single-planar API through the
 *     Video Capture interface.
 *   - The device should support the streaming I/O method.
 *
 * Return true if the above conditions are true. Otherwise, return false */
bool v4l2_verify_dev(int dev_fd);

/* Print capabilities of V4L2 device */
void v4l2_print_caps(int dev_fd);

/* This function converts integer value 'fourcc' to string.
 * Return 'str' (useful when passing the function to 'printf').
 *
 * Note 1: 'fourcc' is created from macro 'v4l2_fourcc' or 'v4l2_fourcc_be':
 * https://github.com/torvalds/linux/blob/master/include/uapi/linux/videodev2.h
 *
 * Note 2: The code is based on function 'std::string fcc2s(__u32 val)':
 * https://git.linuxtv.org/v4l-utils.git/tree/utils/common/v4l2-info.cpp */
char * v4l2_fourcc_to_str(uint32_t fourcc, char str[8]);

/* Print current format of V4L2 device */
void v4l2_print_format(int dev_fd);

/* Print current framerate of V4L2 device */
void v4l2_print_framerate(int dev_fd);

/* This function gets format for V4L2 device */
bool v4l2_get_format(int dev_fd, struct v4l2_format * p_fmt);

/* This function gets streaming parameters for V4L2 device */
bool v4l2_get_stream_params(int dev_fd, struct v4l2_streamparm * p_params);

/* This function sets format for V4L2 device.
 * Return true if the requested format is set successfully */
bool v4l2_set_format(int dev_fd,
                     uint32_t img_width, uint32_t img_height,
                     uint32_t pix_fmt, enum v4l2_field field);

/* This function sets framerate for V4L2 device.
 * Return true if the requested framerate is set successfully */
bool v4l2_set_framerate(int dev_fd, uint32_t framerate);

/* Export dmabuf from V4L2 device */
bool v4l2_export_dmabuf(int dev_fd, uint32_t index, v4l2_dmabuf_exp_t * p_buf);

/* Allocate dmabufs for V4L2 device.
 *
 * The function returns an array of allocated dmabufs and
 * updates 'p_count' if length of the array is smaller than 'p_count'.
 *
 * If error, it will return NULL.
 * Note: The array must be freed when no longer used */
v4l2_dmabuf_exp_t * v4l2_alloc_dmabufs(int dev_fd, uint32_t * p_count);

/* Free dmabufs (allocated by 'v4l2_alloc_dmabufs') */
void v4l2_dealloc_dmabufs(v4l2_dmabuf_exp_t * p_bufs, uint32_t count);

/* This function enqueues a buffer with sequence number 'index' to V4L2 device.
 * The value 'index' ranges from zero to the number of buffers allocated with
 * the ioctl 'VIDIOC_REQBUFS' (struct 'v4l2_requestbuffers::count') minus one.
 *
 * Return true if the buffer is enqueued successfully */
bool v4l2_enqueue_buf(int dev_fd, uint32_t index);

/* This function enqueues buffers to V4L2 device.
 *
 * Return true if all buffers are enqueued successfully.
 * Otherwise, return false */
bool v4l2_enqueue_bufs(int dev_fd, uint32_t count);

/* This function dequeues a buffer from V4L2 device.
 * Return true and update structure 'v4l2_buffer' pointed by 'p_buf' if
 * successful */
bool v4l2_dequeue_buf(int dev_fd, struct v4l2_buffer * p_buf);

/* This function enables capturing process on V4L2 device */
bool v4l2_enable_capturing(int dev_fd);

/* This function disables capturing process on V4L2 device */
bool v4l2_disable_capturing(int dev_fd);

/************************ FOR QUEUE (NOT THREAD-SAFE) *************************/

/* Create an empty queue whose size is ('elm_cnt' * 'elm_size') bytes */
queue_t queue_create_empty(uint32_t elm_cnt, uint32_t elm_size);

/* Create a full queue whose content is a shallow copy of 'p_array' */
queue_t queue_create_full(const void * p_array,
                          uint32_t elm_cnt, uint32_t elm_size);

/* Delete queue.
 * Note: This function will deallocate 'p_queue->p_array' */
void queue_delete(queue_t * p_queue);

/* Check if queue is empty or not? */
bool queue_is_empty(const queue_t * p_queue);

/* Check if queue is full or not? */
bool queue_is_full(const queue_t * p_queue);

/* Remove an element from the front of the queue. Then, return its address.
 * Note: Return non-NULL value if successful */
void * queue_dequeue(queue_t * p_queue);

/* Add a shallow copy of element (pointed by 'p_elm') to the end of the queue.
 * Note: Size of the element must be equal to 'p_queue->elm_size' */
bool queue_enqueue(queue_t * p_queue, const void * p_elm);

/********************************** FOR OMX ***********************************/

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
 * Note: This is a blocking call */
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
 * shall handle any errors generated internally */
OMX_ERRORTYPE omx_empty_buffer_done(OMX_HANDLETYPE hComponent,
                                    OMX_PTR pAppData,
                                    OMX_BUFFERHEADERTYPE * pBuffer);

/* The method is used to return filled buffers from an output port back to
 * the application for emptying and then reuse.
 *
 * This is a blocking call so the application should not attempt to empty
 * the buffers during this call, but should queue them and empty them in
 * another thread.
 *
 * Callbacks should not return an error to the component, so the application
 * shall handle any errors generated internally */
OMX_ERRORTYPE omx_fill_buffer_done(OMX_HANDLETYPE hComponent,
                                   OMX_PTR pAppData,
                                   OMX_BUFFERHEADERTYPE * pBuffer);

/* This method blocks calling thread until the component is in state 'state'
 * (based on section 3.2.2.13.2 in OMX IL specification 1.1.2) */
void omx_wait_state(OMX_HANDLETYPE handle, OMX_STATETYPE state);

/* Convert 'OMX_STATETYPE' to string.
 * Note: The string must be freed when no longer used */
char * omx_state_to_str(OMX_STATETYPE state);

/* Print media component's role */
void omx_print_mc_role(OMX_HANDLETYPE handle);

/* Get port's structure 'OMX_PARAM_PORTDEFINITIONTYPE'.
 * Note: 'port_idx' should be 0 (input port) or 1 (output port) */
bool omx_get_port(OMX_HANDLETYPE handle, OMX_U32 port_idx,
                  OMX_PARAM_PORTDEFINITIONTYPE * p_port);

/* Get video encode bitrate control for port at 'port_idx' */
bool omx_get_bitrate_ctrl(OMX_HANDLETYPE handle, OMX_U32 port_idx,
                          OMX_VIDEO_PARAM_BITRATETYPE * p_ctrl);

/* Set raw format to input port's structure 'OMX_PARAM_PORTDEFINITIONTYPE' */
bool omx_set_in_port_fmt(OMX_HANDLETYPE handle,
                         OMX_U32 frame_width, OMX_U32 frame_height,
                         OMX_COLOR_FORMATTYPE color_fmt, OMX_U32 framerate);

/* Set H.264 format and bitrate to output port's structure
 * 'OMX_PARAM_PORTDEFINITIONTYPE' */
bool omx_set_out_port_fmt(OMX_HANDLETYPE handle, OMX_U32 bitrate,
                          OMX_VIDEO_CODINGTYPE compression_fmt);

/* Set 'buf_cnt' buffers to port 'port_idx' */
bool omx_set_port_buf_cnt(OMX_HANDLETYPE handle,
                          OMX_U32 port_idx, OMX_U32 buf_cnt);

/* Use buffers in 'p_bufs' to allocate buffer headers for port at 'port_idx'.
 * Note: 'p_bufs[index].size' must be equal to 'nBufferSize' of port */
OMX_BUFFERHEADERTYPE ** omx_use_buffers(OMX_HANDLETYPE handle, OMX_U32 port_idx,
                                        mmngr_buf_t * p_bufs, uint32_t count);

/* Allocate buffers and buffer headers for port at 'port_idx' */
OMX_BUFFERHEADERTYPE ** omx_alloc_buffers(OMX_HANDLETYPE handle,
                                          OMX_U32 port_idx);

/* Free 'count' elements in 'pp_bufs' */
void omx_dealloc_port_bufs(OMX_HANDLETYPE handle, OMX_U32 port_idx,
                           OMX_BUFFERHEADERTYPE ** pp_bufs, uint32_t count);

/* Free 'nBufferCountActual' elements in 'pp_bufs'.
 * Note: Make sure the length of 'pp_bufs' is equal to 'nBufferCountActual' */
void omx_dealloc_all_port_bufs(OMX_HANDLETYPE handle, OMX_U32 port_idx,
                               OMX_BUFFERHEADERTYPE ** pp_bufs);

/* Get index of element 'p_buf' in array 'pp_bufs'.
 * Return non-negative value if successful */
int omx_get_index(OMX_BUFFERHEADERTYPE * p_buf,
                  OMX_BUFFERHEADERTYPE ** pp_bufs, uint32_t count);

/* Send buffers in 'pp_bufs' to output port */
bool omx_fill_buffers(OMX_HANDLETYPE handle,
                      OMX_BUFFERHEADERTYPE ** pp_bufs, uint32_t count);

/********************************** FOR EGL ***********************************/

/* This function obtains EGL display connection.
 * Return a value other than 'EGL_NO_DISPLAY' if successful */
EGLDisplay egl_create_display();

/* Release current context and terminate EGL display connection */
void egl_delete_display(EGLDisplay display);

/* Check if 'p_name' is a supported extension to EGL */
bool egl_is_ext_supported(EGLDisplay display, const char * p_name);

/* Initialize EGL extension functions.
 * Return true if all functions are supported at runtime */
bool egl_init_ext_funcs(EGLDisplay display);

/* Create YUYV EGLImage from a dmabuf file descriptor.
 * Return a value other than 'EGL_NO_IMAGE_KHR' if successful */
EGLImageKHR egl_create_yuyv_image(EGLDisplay display, uint32_t img_width,
                                  uint32_t img_height, int dmabuf_fd);

/* Create YUYV EGLImage objects from an array of 'v4l2_dmabuf_exp_t' structs.
 * Return an array of EGLImage objects with the number of elements 'cnt' */
EGLImageKHR * egl_create_yuyv_images(EGLDisplay display,
                                     uint32_t img_width, uint32_t img_height,
                                     v4l2_dmabuf_exp_t * p_bufs, uint32_t cnt);

/* Create NV12 EGLImage from dmabuf file descriptors.
 * Return a value other than 'EGL_NO_IMAGE_KHR' if successful */
EGLImageKHR egl_create_nv12_image(EGLDisplay display,
                                  uint32_t img_width, uint32_t img_height,
                                  int y_dmabuf_fd, int uv_dmabuf_fd);

/* Create NV12 EGLImage objects from an array of 'mmngr_buf_t' structs.
 * Return an array of EGLImage objects with the number of elements 'count' */
EGLImageKHR * egl_create_nv12_images(EGLDisplay display,
                                     uint32_t img_width, uint32_t img_height,
                                     mmngr_buf_t * p_bufs, uint32_t count);

/* Delete an array of EGLImage objects.
 * Note: This function will deallocate array 'p_imgs' */
void egl_delete_images(EGLDisplay display,
                       EGLImageKHR * p_imgs, uint32_t count);

/******************************* FOR OPENGL ES ********************************/

/* Compile the shader from file 'p_file' */
GLuint gl_create_shader(const char * p_file, GLenum type);

/* Create program from 2 existing vertex shader and fragment shader objects */
GLuint gl_create_prog_from_objs(GLuint vs_object, GLuint fs_object);

/* Create program from file 'p_vs_file' and file 'p_fs_file' */
GLuint gl_create_prog_from_src(const char * p_vs_file, const char * p_fs_file);

/* Check if extension string 'p_name' is supported by the implementation */
bool gl_is_ext_supported(const char * p_name);

/* Initialize OpenGL ES extension functions.
 * Return true if all functions are supported at runtime */
bool gl_init_ext_funcs();

/* Create texture from EGLImage.
 * Return texture's ID (other than 0) if successful.
 *
 * Note: The texture is unbound after calling this function.
 *       Please make sure to bind it when you render it ('glDrawElements')
 *       or adjust its parameters ('glTexParameteri') */
GLuint gl_create_texture(EGLImageKHR image);

/* Create textures from an array of EGLImage objects.
 * Return an array of textures with the number of elements 'count' */
GLuint * gl_create_textures(EGLImageKHR * p_images, uint32_t count);

/* Delete textures.
 * Note: This function will deallocate array 'p_textures' */
void gl_delete_textures(GLuint * p_textures, uint32_t count);

/* Create framebuffer from texture.
 * Return a value other than 0 if successful */
GLuint gl_create_framebuffer(GLuint texture);

/* Create framebuffers from an array of textures.
 * Return an array of framebuffers with the number of elements 'count' */
GLuint * gl_create_framebuffers(GLuint * p_textures, uint32_t count);

/* Delete framebuffers.
 * Note: This function will deallocate array 'p_fbs' */
void gl_delete_framebuffers(GLuint * p_fbs, uint32_t count);

/* Create resources for OpenGL ES */
gl_resources_t gl_create_resources();

/* Delete resources for OpenGL ES */
void gl_delete_resources(gl_resources_t res);

/* Draw rectangle */
void gl_draw_rectangle(gl_resources_t res);

/* Convert YUYV texture to NV12 texture */
void gl_conv_yuyv_to_nv12(GLuint yuyv_texture, gl_resources_t res);

/********************************* FOR MMNGR **********************************/

/* Allocate NV12 buffers and export dmabufs.
 * Return an array of 'mmngr_buf_t' structs.
 *
 * Note: The array must be freed when no longer used */
mmngr_buf_t * mmngr_alloc_nv12_dmabufs(uint32_t count, size_t nv12_size);

/* Deallocate dmabufs (allocated by 'mmngr_alloc_nv12_dmabufs') */
void mmngr_dealloc_nv12_dmabufs(mmngr_buf_t * p_bufs, uint32_t count);

/******************************** FOR THREADS *********************************/

/* Try to call 'OMX_EmptyThisBuffer' whenerver there is an available
 * input buffer and interrupt signal is not raised.
 *
 * Warning: Only create 1 thread for this routine */
void * thread_input(void * p_param);

/* Try to call 'OMX_FillThisBuffer' whenever there is an available
 * output buffer and End-of-Stream event is not raised.
 *
 * Warning: Only create 1 thread for this routine */
void * thread_output(void * p_param);

/****************************** FOR FILE ACCESS *******************************/

/* Read file's contents.
 * Note: The content must be freed when no longer used */
char * file_read_str(const char * p_name);

/* Write data to a file.
 * Note: The data is not freed by this function */
void file_write_buffer(const char * p_name, const char * p_buffer, size_t size);

/***************************** FOR PAGE ALIGNMENT *****************************/

/* Get smallest integral value not less than 'size' and is a multiple of
 * page size (4096 bytes) */
size_t page_size_get_aligned_size(size_t size);

/* Check if 'size' is aligned to page size or not? */
bool page_size_is_size_aligned(size_t size);

/***************************** FOR STRING SEARCH ******************************/

/* This function converts 'p_str' to upper case.
 * Return 'p_str' (useful when passing the function to another function) */
char * str_to_uppercase(char * p_str);

/* Find 'p_str' in 'p_str_arr' separated by 'p_delim_str'.
 *
 * For example:
 *   1. str_find_whole_str("Hello World, Friends", ", ", "Friends")
 *      -> Extract words: 'Hello', 'World', and 'Friends' from colons
 *         and spaces. Then, return true because 'Friends' matches.
 *
 *   2. str_find_whole_str("Hello World, Friends", ", ", "Friend")
 *      -> Extract words: 'Hello', 'World', and 'Friends' from colons
 *         and spaces. Then, return false because 'Friend' does not match.
 *
 * Note: The function is not case sensitive */
bool str_find_whole_str(const char * p_str_arr,
                        const char * p_delim_str, const char * p_str);

/****************************** FOR ERROR OUTPUT ******************************/

/* Print error based on 'errno' */
void errno_print();

/******************************************************************************
 *                               MAIN FUNCTION                                *
 ******************************************************************************/

int main(int argc, char ** pp_argv)
{
    /* Interrupt signal */
    struct sigaction sig_act;

    /* File descriptor of camera */
    int cam_fd = -1;

    /* Data format of camera */
    struct v4l2_format cam_fmt;

    /* YUYV buffers */
    uint32_t index   = 0;
    uint32_t buf_cnt = 0;

    v4l2_dmabuf_exp_t * p_yuyv_bufs = NULL;

    /* NV12 buffers */
    mmngr_buf_t * p_nv12_bufs = NULL;

    /* Handle of media component */
    OMX_HANDLETYPE handle;

    /* Callbacks used by media component */
    OMX_CALLBACKTYPE callbacks =
    {
        .EventHandler    = omx_event_handler,
        .EmptyBufferDone = omx_empty_buffer_done,
        .FillBufferDone  = omx_fill_buffer_done
    };

    /* Buffers for input and output ports */
    OMX_BUFFERHEADERTYPE ** pp_in_bufs  = NULL;
    OMX_BUFFERHEADERTYPE ** pp_out_bufs = NULL;

    /* Queues for buffers of input and output ports */
    queue_t in_queue;
    queue_t out_queue;

    /* Shared data between OMX's callbacks */
    omx_data_t omx_data;

    /* Data for threads */
    in_data_t  in_data;
    out_data_t out_data;

    /* Threads */
    pthread_t thread_in;
    pthread_t thread_out;

    /* Mutexes for condition variables */
    pthread_mutex_t mut_in;
    pthread_mutex_t mut_out;

    /* Condition variables */
    pthread_cond_t cond_in_available;
    pthread_cond_t cond_out_available;

    /**************************************************************************
     *                STEP 1: SET UP INTERRUPT SIGNAL HANDLER                 *
     **************************************************************************/

    sig_act.sa_sigaction = sigint_handler;
    sig_act.sa_flags     = SA_SIGINFO | SA_RESTART;

    sigaction(SIGINT,  &sig_act, NULL);
    sigaction(SIGTERM, &sig_act, NULL);
    sigaction(SIGQUIT, &sig_act, NULL);

    /**************************************************************************
     *                       STEP 2: SET UP V4L2 DEVICE                       *
     **************************************************************************/

    /* Open camera */
    cam_fd = v4l2_open_dev(USB_CAMERA_FD);
    assert(cam_fd != -1);

    /* Verify camera */
    assert(v4l2_verify_dev(cam_fd));

    /* Print information of camera */
    v4l2_print_caps(cam_fd);

    /* Set format for camera */
    assert(v4l2_set_format(cam_fd,
                           FRAME_WIDTH_IN_PIXELS, FRAME_HEIGHT_IN_PIXELS,
                           V4L2_PIX_FMT_YUYV, V4L2_FIELD_NONE));

    /* Confirm new format */
    assert(v4l2_get_format(cam_fd, &cam_fmt));
    assert(cam_fmt.fmt.pix.bytesperline == YUYV_FRAME_WIDTH_IN_BYTES);
    assert(cam_fmt.fmt.pix.sizeimage    == YUYV_FRAME_SIZE_IN_BYTES);
    assert(cam_fmt.fmt.pix.pixelformat  == V4L2_PIX_FMT_YUYV);
    assert(cam_fmt.fmt.pix.field        == V4L2_FIELD_NONE);

    /* Set framerate for camera */
    assert(v4l2_set_framerate(cam_fd, FRAMERATE));

    /* Print format of camera to console */
    v4l2_print_format(cam_fd);

    /* Print framerate of camera to console */
    v4l2_print_framerate(cam_fd);

    /**************************************************************************
     *             STEP 3: ALLOCATE YUYV BUFFERS FROM V4L2 DEVICE             *
     **************************************************************************/

    buf_cnt = YUYV_BUFFER_COUNT;
    p_yuyv_bufs = v4l2_alloc_dmabufs(cam_fd, &buf_cnt);

    assert(buf_cnt == YUYV_BUFFER_COUNT);

    for (index = 0; index < YUYV_BUFFER_COUNT; index++)
    {
        assert(p_yuyv_bufs[index].size == YUYV_FRAME_SIZE_IN_BYTES);
    }

    /**************************************************************************
     *               STEP 4: USE MMNGR TO ALLOCATE NV12 BUFFERS               *
     **************************************************************************/

    p_nv12_bufs = mmngr_alloc_nv12_dmabufs(NV12_BUFFER_COUNT,
                                           NV12_FRAME_SIZE_IN_BYTES);
    assert(p_nv12_bufs != NULL);

    /**************************************************************************
     *                         STEP 5: SET UP OMX IL                          *
     **************************************************************************/

    /* Initialize OMX IL core */
    assert(OMX_Init() == OMX_ErrorNone);

    /* Locate Renesas's H.264 encoder.
     * If successful, the component will be in state LOADED */
    assert(OMX_ErrorNone == OMX_GetHandle(&handle,
                                          RENESAS_VIDEO_ENCODER_NAME,
                                          (OMX_PTR)&omx_data, &callbacks));

    /* Print role of the component to console */
    omx_print_mc_role(handle);

    /* Configure input port */
    assert(omx_set_in_port_fmt(handle,
                               FRAME_WIDTH_IN_PIXELS, FRAME_HEIGHT_IN_PIXELS,
                               OMX_COLOR_FormatYUV420SemiPlanar, FRAMERATE));

    assert(omx_set_port_buf_cnt(handle, 0, NV12_BUFFER_COUNT));

    /* Configure output port */
    assert(omx_set_out_port_fmt(handle, H264_BITRATE, OMX_VIDEO_CodingAVC));

    assert(omx_set_port_buf_cnt(handle, 1, H264_BUFFER_COUNT));

    /* Transition into state IDLE */
    assert(OMX_ErrorNone == OMX_SendCommand(handle,
                                            OMX_CommandStateSet,
                                            OMX_StateIdle, NULL));

    /**************************************************************************
     *                STEP 6: ALLOCATE BUFFERS FOR INPUT PORT                 *
     **************************************************************************/

    pp_in_bufs = omx_use_buffers(handle, 0, p_nv12_bufs, NV12_BUFFER_COUNT);
    assert(pp_in_bufs != NULL);

    /* Create queue from these buffers */
    in_queue = queue_create_full(pp_in_bufs, NV12_BUFFER_COUNT,
                                 sizeof(OMX_BUFFERHEADERTYPE *));

    /**************************************************************************
     *                STEP 7: ALLOCATE BUFFERS FOR OUTPUT PORT                *
     **************************************************************************/

    pp_out_bufs = omx_alloc_buffers(handle, 1);
    assert(pp_out_bufs != NULL);

    /* Create empty queue whose size is equal to 'pp_out_bufs' */
    out_queue = queue_create_empty(H264_BUFFER_COUNT,
                                   sizeof(OMX_BUFFERHEADERTYPE *));

    /* Wait until the component is in state IDLE */
    omx_wait_state(handle, OMX_StateIdle);

    /**************************************************************************
     *             STEP 8: CREATE MUTEXES AND CONDITION VARIABLES             *
     **************************************************************************/

    pthread_mutex_init(&mut_in, NULL);
    pthread_mutex_init(&mut_out, NULL);

    pthread_cond_init(&cond_in_available, NULL);
    pthread_cond_init(&cond_out_available, NULL);

    /**************************************************************************
     *          STEP 9: PREPARE SHARED DATA BETWEEN OMX'S CALLBACKS           *
     **************************************************************************/

    omx_data.p_in_queue           = &in_queue;
    omx_data.p_out_queue          = &out_queue;
    omx_data.p_mut_in             = &mut_in;
    omx_data.p_mut_out            = &mut_out;
    omx_data.p_cond_in_available  = &cond_in_available;
    omx_data.p_cond_out_available = &cond_out_available;

    /**************************************************************************
     *            STEP 10: MAKE OMX READY TO SEND/RECEIVE BUFFERS             *
     **************************************************************************/

    /* Transition into state EXECUTING */
    assert(OMX_ErrorNone == OMX_SendCommand(handle,
                                            OMX_CommandStateSet,
                                            OMX_StateExecuting, NULL));
    omx_wait_state(handle, OMX_StateExecuting);

    /* Send buffers in 'pp_out_bufs' to output port */
    assert(omx_fill_buffers(handle, pp_out_bufs, H264_BUFFER_COUNT));

    /**************************************************************************
     *                STEP 11: MAKE V4L2 READY TO CAPTURE DATA                *
     **************************************************************************/

    /* For capturing applications, it is customary to first enqueue all
     * mapped buffers, then to start capturing and enter the read loop.
     *
     * https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/mmap.html */
    assert(v4l2_enqueue_bufs(cam_fd, YUYV_BUFFER_COUNT));

    /* Start capturing */
    assert(v4l2_enable_capturing(cam_fd));

    /**************************************************************************
     *                 STEP 12: PREPARE DATA FOR INPUT THREAD                 *
     **************************************************************************/

    in_data.cam_fd           = cam_fd;
    in_data.p_yuyv_bufs      = p_yuyv_bufs;
    in_data.p_nv12_bufs      = p_nv12_bufs;
    in_data.handle           = handle;
    in_data.pp_bufs          = pp_in_bufs;
    in_data.p_queue          = &in_queue;
    in_data.p_mutex          = &mut_in;
    in_data.p_cond_available = &cond_in_available;

    /**************************************************************************
     *                STEP 13: PREPARE DATA FOR OUTPUT THREAD                 *
     **************************************************************************/

    out_data.handle           = handle;
    out_data.p_queue          = &out_queue;
    out_data.p_mutex          = &mut_out;
    out_data.p_cond_available = &cond_out_available;

    /**************************************************************************
     *                          STEP 14: RUN THREADS                          *
     **************************************************************************/

    pthread_create(&thread_in, NULL, thread_input, &in_data);
    pthread_create(&thread_out, NULL, thread_output, &out_data);

    /**************************************************************************
     *                 STEP 15: WAIT UNTIL END-OF-STREAM EVENT                 *
     **************************************************************************/

    pthread_join(thread_in, NULL);
    pthread_join(thread_out, NULL);

    /* Stop capturing */
    assert(v4l2_disable_capturing(cam_fd));

    /**************************************************************************
     *                    STEP 16: CLEAN UP OMX' RESOURCES                    *
     **************************************************************************/

    pthread_mutex_destroy(&mut_in);
    pthread_mutex_destroy(&mut_out);

    pthread_cond_destroy(&cond_in_available);
    pthread_cond_destroy(&cond_out_available);

    /* Transition into state IDLE */
    assert(OMX_ErrorNone == OMX_SendCommand(handle,
                                            OMX_CommandStateSet,
                                            OMX_StateIdle, NULL));
    omx_wait_state(handle, OMX_StateIdle);

    /* Transition into state LOADED */
    assert(OMX_ErrorNone == OMX_SendCommand(handle,
                                            OMX_CommandStateSet,
                                            OMX_StateLoaded, NULL));

    /* Release buffers and buffer headers from the component.
     *
     * The component shall free only the buffer headers if it allocated only
     * the buffer headers ('OMX_UseBuffer').
     *
     * The component shall free both the buffers and the buffer headers if it
     * allocated both the buffers and buffer headers ('OMX_AllocateBuffer') */
    omx_dealloc_all_port_bufs(handle, 0, pp_in_bufs);
    omx_dealloc_all_port_bufs(handle, 1, pp_out_bufs);

    queue_delete(&in_queue);
    queue_delete(&out_queue);

    /* Wait until the component is in state LOADED */
    omx_wait_state(handle, OMX_StateLoaded);

    /* Free the component's handle */
    assert(OMX_FreeHandle(handle) == OMX_ErrorNone);

    /* Deinitialize OMX IL core */
    assert(OMX_Deinit() == OMX_ErrorNone);

    /* Deallocate NV12 buffers */
    mmngr_dealloc_nv12_dmabufs(p_nv12_bufs, NV12_BUFFER_COUNT);

    /**************************************************************************
     *                   STEP 17: CLEAN UP V4L2'S RESOURCES                   *
     **************************************************************************/

    /* Clean up YUYV buffers */
    v4l2_dealloc_dmabufs(p_yuyv_bufs, YUYV_BUFFER_COUNT);

    /* Close the camera */
    close(cam_fd);

    return 0;
}

/******************************************************************************
 *                            FUNCTION DEFINITIONS                            *
 ******************************************************************************/

/**************************** FOR SIGNAL HANDLING *****************************/

void sigint_handler(int signum, siginfo_t * p_info, void * ptr)
{
    g_int_signal = 1;
}

/********************************** FOR V4L2 **********************************/

int v4l2_open_dev(const char * p_name)
{
    int dev_fd = -1;
    struct stat file_st;

    /* Check parameter */
    assert(p_name != NULL);

    /* Check if 'p_name' exists or not? */
    if (stat(p_name, &file_st) == -1)
    {
        errno_print();
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
        errno_print();
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
        errno_print();
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
        errno_print();
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
        errno_print();
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
        errno_print();
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
        errno_print();
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
        errno_print();
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
        errno_print();
        return false;
    }

    /* Export the buffer as a dmabuf file descriptor */
    memset(&expbuf, 0, sizeof(struct v4l2_exportbuffer));
    expbuf.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    expbuf.index = index;

    /* https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/vidioc-expbuf.html */
    if (ioctl(dev_fd, VIDIOC_EXPBUF, &expbuf) == -1)
    {
        errno_print();
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

        errno_print();
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
        errno_print();
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
        errno_print();
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
        errno_print();
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
        errno_print();
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
        errno_print();
        return false;
    }

    return true;
}

/************************ FOR QUEUE (NOT THREAD-SAFE) *************************/

queue_t queue_create_empty(uint32_t elm_cnt, uint32_t elm_size)
{
    queue_t queue;

    /* Front = -2
     * ↓
     * ┌┄┄┄┄┬┄┄┄┄┬────┬────┬────┬────┬────┬────┬────┬────┐ elm_cnt  = 4
     * ┊    ┊    │    │    │    │    │    │    │    │    │ elm_size = 2
     * └┄┄┄┄┴┄┄┄┄┴────┴────┴────┴────┴────┴────┴────┴────┘
     * ↑         ├─────────┼─────────┼─────────┼─────────┤
     * Rear = -2  Element 1 Element 2 Element 3 Element 4 */

    /* Check parameters */
    assert((elm_cnt > 0) && (elm_size > 0));

    queue.elm_cnt  = elm_cnt;
    queue.elm_size = elm_size;
    queue.p_array  = calloc(elm_cnt, elm_size);

    /* Mark the queue as empty */
    queue.front_idx = -1 * elm_size;
    queue.rear_idx  = -1 * elm_size;

    return queue;
}

queue_t queue_create_full(const void * p_array,
                          uint32_t elm_cnt, uint32_t elm_size)
{
    queue_t queue;

    /* Front = 0                           Rear = 6
     * ↓                                   ↓
     * ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐ elm_cnt  = 4
     * │  A  │  B  │  C  │  D  │  E  │  F  │  G  │  H  │ elm_size = 2
     * └─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘
     * ├───────────┼───────────┼───────────┼───────────┤
     *   Element 1   Element 2   Element 3   Element 4 */

    /* Check parameters */
    assert((p_array != NULL) && (elm_cnt > 0) && (elm_size > 0));

    /* Create empty queue */
    queue = queue_create_empty(elm_cnt, elm_size);

    /* Copy data from 'p_array' to 'queue.p_array' */
    memcpy(queue.p_array, p_array, elm_cnt * elm_size);

    /* Mark the queue as full */
    queue.front_idx = 0;
    queue.rear_idx  = (elm_cnt - 1) * elm_size;

    return queue;
}

void queue_delete(queue_t * p_queue)
{
    /* Check parameter */
    assert(p_queue != NULL);

    /* Free entire array */
    free(p_queue->p_array);

    /* Deinitialize structure */
    p_queue->p_array  = NULL;
    p_queue->elm_cnt  = 0;
    p_queue->elm_size = 0;

    p_queue->front_idx = -1;
    p_queue->rear_idx  = -1;
}

bool queue_is_empty(const queue_t * p_queue)
{
    /* Check parameter */
    assert(p_queue != NULL);

    /* Case 1: After calling function 'queue_create_empty'.
     * Case 2: After dequeuing the last element (function 'queue_dequeue'):
     *
     *   Code snippets:
     *
     *     int16_t array[2] = { 1, 2 };
     *     int16_t value1 = 0;
     *     int16_t value2 = 0;
     *
     *     queue_t queue = queue_create_empty(3, sizeof(int16_t));
     *     queue_enqueue(&queue, &(array[0]));
     *     queue_enqueue(&queue, &(array[1]));
     *
     *     value1 = *((int16_t *)queue_dequeue(&queue));
     *     value2 = *((int16_t *)queue_dequeue(&queue));
     *
     *   Result:
     *
     *     Front = -2
     *     ↓
     *     ┌┄┄┄┄┬┄┄┄┄┬─────┬─────┬─────┬─────┬─────┬─────┐ elm_cnt  = 3
     *     │    ┊    │  A  │  B  │  C  │  D  │     │     │ elm_size = 2
     *     └┄┄┄┄┴┄┄┄┄┴─────┴─────┴─────┴─────┴─────┴─────┘
     *     ↑         ├───────────┼───────────┼───────────┤
     *     Rear = -2   Element 1   Element 2   Element 3 */

    return (p_queue->front_idx == (-1 * p_queue->elm_size));
}

bool queue_is_full(const queue_t * p_queue)
{
    /* Check parameter */
    assert(p_queue != NULL);

    /* Case 1: When latest element is at the end of queue and the app did not
     * dequeue any elements from it (see function 'queue_create_full').
     *
     * Case 2: When latest element is right after oldest element:
     *
     *   Code snippets:
     *
     *     int16_t array[4] = { 1, 2, 3, 4 };
     *
     *     queue_t queue = queue_create_full(array,
     *                                       sizeof(array) / sizeof(int16_t),
     *                                       sizeof(int16_t));
     *     int16_t value1 = *((int16_t *)queue_dequeue(&queue));
     *
     *     int16_t value2 = 5;
     *     queue_enqueue(&queue, &value2);
     *
     *   Result:
     *
     *     Rear = 0    Front = 2
     *     ↓           ↓
     *     ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐ elm_cnt  = 4
     *     │  I  │  K  │  C  │  D  │  E  │  F  │  G  │  H  │ elm_size = 2
     *     └─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘
     *     ├───────────┼───────────┼───────────┼───────────┤
     *       Element 1   Element 2   Element 3   Element 4 */

    const uint32_t last_idx = (p_queue->elm_cnt - 1) * p_queue->elm_size;

    return ((p_queue->front_idx == 0) && (p_queue->rear_idx == last_idx)) ||
           (p_queue->front_idx == (p_queue->rear_idx + p_queue->elm_size));
}

void * queue_dequeue(queue_t * p_queue)
{
    void * p_elm = NULL;

    if (queue_is_empty(p_queue))
    {
        printf("Error: Queue is empty\n");
    }
    else
    {
        p_elm = p_queue->p_array + p_queue->front_idx;

        if (p_queue->front_idx == p_queue->rear_idx)
        {
            /* Initialize the queue if there is only 1 element in it */
            p_queue->front_idx = -1 * p_queue->elm_size;
            p_queue->rear_idx  = -1 * p_queue->elm_size;
        }
        else
        {
            /* Adjust 'front_idx' for next function call 'queue_dequeue' */
            p_queue->front_idx = (p_queue->front_idx + p_queue->elm_size) %
                                 (p_queue->elm_cnt * p_queue->elm_size);
        }
    }

    return p_elm;
}

bool queue_enqueue(queue_t * p_queue, const void * p_elm)
{
    bool is_success = true;

    if (queue_is_full(p_queue))
    {
        printf("Error: Queue is full\n");
        is_success = false;
    }
    else
    {
        if (p_queue->front_idx == (-1 * p_queue->elm_size))
        {
            /* The first element is about to be added to the queue.
             * Let's adjust 'front_idx' for next function call 'queue_dequeue'
             */
            p_queue->front_idx = 0;
        }

        /* Add element to the end of the queue */
        p_queue->rear_idx = (p_queue->rear_idx + p_queue->elm_size) %
                            (p_queue->elm_cnt * p_queue->elm_size);

        memcpy(p_queue->p_array + p_queue->rear_idx, p_elm, p_queue->elm_size);
    }

    return is_success;
}

/********************************** FOR OMX ***********************************/

OMX_ERRORTYPE omx_event_handler(OMX_HANDLETYPE hComponent, OMX_PTR pAppData,
                                OMX_EVENTTYPE eEvent, OMX_U32 nData1,
                                OMX_U32 nData2, OMX_PTR pEventData)
{
    char * p_state_str = NULL;

    switch (eEvent)
    {
        case OMX_EventCmdComplete:
        {
            if (nData1 == OMX_CommandStateSet)
            {
                p_state_str = omx_state_to_str((OMX_STATETYPE)nData2);
                if (p_state_str != NULL)
                {
                    /* Print OMX state */
                    printf("OMX state: '%s'\n", p_state_str);
                    free(p_state_str);
                }
            }
        }
        break;

        case OMX_EventBufferFlag:
        {
            /* The buffer contains the last output picture data */
            printf("OMX event: 'End-of-Stream'\n");
        }
        break;

        case OMX_EventError:
        {
            /* Section 2.1.2 in document 'R01USxxxxEJxxxx_vecmn_v1.0.pdf' */
            printf("OMX error event: '0x%x'\n", nData1);
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

OMX_ERRORTYPE omx_empty_buffer_done(OMX_HANDLETYPE hComponent,
                                    OMX_PTR pAppData,
                                    OMX_BUFFERHEADERTYPE * pBuffer)
{
    omx_data_t * p_data = (omx_data_t *)pAppData;

    /* Check parameter */
    assert(p_data != NULL);

    if (pBuffer != NULL)
    {
        assert(pthread_mutex_lock(p_data->p_mut_in) == 0);

        /* At this point, the queue must not be full */
        assert(!queue_is_full(p_data->p_in_queue));

        /* Add 'pBuffer' to the queue */
        assert(queue_enqueue(p_data->p_in_queue, &pBuffer));

        /* Now, there is an element inside the queue.
         * Input thread should woken up in case it's sleeping */
        assert(pthread_cond_signal(p_data->p_cond_in_available) == 0);

        assert(pthread_mutex_unlock(p_data->p_mut_in) == 0);
    }

    printf("EmptyBufferDone exited\n");
    return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_fill_buffer_done(OMX_HANDLETYPE hComponent,
                                   OMX_PTR pAppData,
                                   OMX_BUFFERHEADERTYPE * pBuffer)
{
    omx_data_t * p_data = (omx_data_t *)pAppData;

    /* Check parameter */
    assert(p_data != NULL);

    if ((pBuffer != NULL) && (pBuffer->nFilledLen > 0))
    {
        assert(pthread_mutex_lock(p_data->p_mut_out) == 0);

        /* Add this point, the queue must not be full */
        assert(!queue_is_full(p_data->p_out_queue));

        /* Add 'pBuffer' to the queue */
        assert(queue_enqueue(p_data->p_out_queue, &pBuffer));

        /* Now, there is an element inside the queue.
         * Output thread should woken up in case it's sleeping */
        assert(pthread_cond_signal(p_data->p_cond_out_available) == 0);

        assert(pthread_mutex_unlock(p_data->p_mut_out) == 0);
    }

    printf("FillBufferDone exited\n");
    return OMX_ErrorNone;
}

void omx_wait_state(OMX_HANDLETYPE handle, OMX_STATETYPE state)
{
    OMX_ERRORTYPE omx_error = OMX_ErrorNone;
    OMX_STATETYPE omx_cur_state = OMX_StateInvalid;

    while (true)
    {
        omx_error = OMX_GetState(handle, &omx_cur_state);

        /* Exit if 'OMX_GetState' returns error */
        if (omx_error != OMX_ErrorNone)
        {
            printf("Error: Failed to get current state of media component\n");
            break;
        }

        /* Exit if OMX has transitioned into desired state */
        if (omx_cur_state == state)
        {
            break;
        }

        /* Wait for 10 ms to avoid wasting CPU cycles */
        usleep(10000);
    }
}

char * omx_state_to_str(OMX_STATETYPE state)
{
    char * p_state_str = NULL;

    struct
    {
        OMX_STATETYPE state;
        const char *  p_state_str;
    }
    state_mapping[] =
    {
        /* The component has detected that its internal data structures are
         * corrupted to the point that it cannot determine its state properly */
        { OMX_StateInvalid, "OMX_StateInvalid" },

        /* The component has been loaded but has not completed initialization.
         * The 'OMX_SetParameter' macro and the 'OMX_GetParameter' macro are
         * the only macros allowed to be sent to the component in this state */
        { OMX_StateLoaded, "OMX_StateLoaded" },

        /* The component initialization has been completed successfully and
         * the component is ready to start */
        { OMX_StateIdle, "OMX_StateIdle" },

        /* The component has accepted the start command and is processing data
         * (if data is available) */
        { OMX_StateExecuting, "OMX_StateExecuting" },

        /* The component has received pause command */
        { OMX_StatePause, "OMX_StatePause" },

        /* The component is waiting for resources, either after preemption or
         * before it gets the resources requested.
         * See OMX IL specification 1.1.2 for complete details */
        { OMX_StateWaitForResources, "OMX_StateWaitForResources" },
    };

    uint32_t len   = sizeof(state_mapping) / sizeof(state_mapping[0]);
    uint32_t index = 0;

    for (index = 0; index < len; index++)
    {
        if (state == state_mapping[index].state)
        {
            p_state_str = malloc(strlen(state_mapping[index].p_state_str) + 1);
            strcpy(p_state_str, state_mapping[index].p_state_str);

            break;
        }
    }

    return p_state_str;
}

void omx_print_mc_role(OMX_HANDLETYPE handle)
{
    OMX_PARAM_COMPONENTROLETYPE role;
    OMX_INIT_STRUCTURE(&role);

    if (OMX_ErrorNone ==
        OMX_GetParameter(handle, OMX_IndexParamStandardComponentRole, &role))
    {
        printf("OMX media component's role: '%s'\n", role.cRole);
    }
}

bool omx_get_port(OMX_HANDLETYPE handle, OMX_U32 port_idx,
                  OMX_PARAM_PORTDEFINITIONTYPE * p_port)
{
    OMX_PARAM_PORTDEFINITIONTYPE port;

    /* Check parameter */
    assert(p_port != NULL);

    OMX_INIT_STRUCTURE(&port);
    port.nPortIndex = port_idx;

    if (OMX_ErrorNone !=
        OMX_GetParameter(handle, OMX_IndexParamPortDefinition, &port))
    {
        printf("Error: Failed to get port at index '%d'\n", port_idx);
        return false;
    }

    /* Copy data from 'port' to 'p_port' */
    memcpy(p_port, &port, sizeof(struct OMX_PARAM_PORTDEFINITIONTYPE));

    return true;
}

bool omx_get_bitrate_ctrl(OMX_HANDLETYPE handle, OMX_U32 port_idx,
                          OMX_VIDEO_PARAM_BITRATETYPE * p_ctrl)
{
    OMX_VIDEO_PARAM_BITRATETYPE ctrl;

    /* Check parameter */
    assert(p_ctrl != NULL);

    OMX_INIT_STRUCTURE(&ctrl);
    ctrl.nPortIndex = port_idx;

    if (OMX_ErrorNone !=
        OMX_GetParameter(handle, OMX_IndexParamVideoBitrate, &ctrl))
    {
        printf("Error: Failed to get bitrate control of port '%d'\n", port_idx);
        return false;
    }

    /* Copy data from 'ctrl' to 'p_ctrl' */
    memcpy(p_ctrl, &ctrl, sizeof(struct OMX_VIDEO_PARAM_BITRATETYPE));

    return true;
}

bool omx_set_in_port_fmt(OMX_HANDLETYPE handle,
                         OMX_U32 frame_width, OMX_U32 frame_height,
                         OMX_COLOR_FORMATTYPE color_fmt, OMX_U32 framerate)
{
    bool is_success = false;
    OMX_PARAM_PORTDEFINITIONTYPE in_port;

    /* Check parameters */
    assert((frame_width > 0) && (frame_height > 0));
    assert(framerate > 0);

    /* Get input port */
    if (omx_get_port(handle, 0, &in_port) == true)
    {
        /* Configure and set new parameters to input port */
        in_port.format.video.nFrameWidth   = frame_width;
        in_port.format.video.nFrameHeight  = frame_height;
        in_port.format.video.nStride       = OMX_STRIDE(frame_width);
        in_port.format.video.nSliceHeight  = OMX_SLICE_HEIGHT(frame_height);
        in_port.format.video.eColorFormat  = color_fmt;
        in_port.format.video.xFramerate    = framerate << 16; /* Q16 format */

        if (OMX_ErrorNone ==
            OMX_SetParameter(handle, OMX_IndexParamPortDefinition, &in_port))
        {
            is_success = true;
        }
    }

    if (!is_success)
    {
        printf("Error: Failed to set input port\n");
    }

    return is_success;
}

bool omx_set_out_port_fmt(OMX_HANDLETYPE handle, OMX_U32 bitrate,
                          OMX_VIDEO_CODINGTYPE compression_fmt)
{
    bool b_set_fmt_ok     = false;
    bool b_set_bitrate_ok = false;

    OMX_VIDEO_PARAM_BITRATETYPE ctrl;
    OMX_PARAM_PORTDEFINITIONTYPE out_port;

    /* Check parameter */
    assert(bitrate > 0);

    /* Get output port */
    if (omx_get_port(handle, 1, &out_port) == true)
    {
        /* Configure and set compression format to output port */
        out_port.format.video.eCompressionFormat = compression_fmt;

        if (OMX_ErrorNone ==
            OMX_SetParameter(handle, OMX_IndexParamPortDefinition, &out_port))
        {
            b_set_fmt_ok = true;
        }
    }

    /* Get video encode bitrate control for output port */
    if (omx_get_bitrate_ctrl(handle, 1, &ctrl) == true)
    {
        /* Configure and set bitrate to output port */
        ctrl.nTargetBitrate = bitrate;
        ctrl.eControlRate   = OMX_Video_ControlRateConstant;

        if (OMX_ErrorNone ==
            OMX_SetParameter(handle, OMX_IndexParamVideoBitrate, &ctrl))
        {
            b_set_bitrate_ok = true;
        }
    }

    if (!b_set_fmt_ok || !b_set_bitrate_ok)
    {
        printf("Error: Failed to set output port\n");
    }

    return (b_set_fmt_ok && b_set_bitrate_ok);
}

bool omx_set_port_buf_cnt(OMX_HANDLETYPE handle,
                          OMX_U32 port_idx, OMX_U32 buf_cnt)
{
    OMX_PARAM_PORTDEFINITIONTYPE port;

    /* Check parameter */
    assert(buf_cnt > 0);

    /* Get port 'port_idx' */
    if (omx_get_port(handle, port_idx, &port) == false)
    {
        return false;
    }

    /* Value 'buf_cnt' must not be less than 'nBufferCountMin' */
    if (buf_cnt < port.nBufferCountMin)
    {
        printf("Error: Port '%d' requires no less than '%d' buffers\n",
                                        port_idx, port.nBufferCountMin);
        return false;
    }

    /* Set the number of buffers that are required on port 'port_idx' */
    port.nBufferCountActual = buf_cnt;

    if (OMX_ErrorNone !=
        OMX_SetParameter(handle, OMX_IndexParamPortDefinition, &port))
    {
        printf("Error: Failed to set port at index '%d'\n", port_idx);
        return false;
    }

    return true;
}

OMX_BUFFERHEADERTYPE ** omx_use_buffers(OMX_HANDLETYPE handle, OMX_U32 port_idx,
                                        mmngr_buf_t * p_bufs, uint32_t count)
{
    uint32_t index = 0;

    OMX_PARAM_PORTDEFINITIONTYPE port;
    OMX_BUFFERHEADERTYPE ** pp_bufs = NULL;

    /* Check parameters */
    assert((p_bufs != NULL) && (count > 0));

    /* Get port */
    if (omx_get_port(handle, port_idx, &port) == false)
    {
        return NULL;
    }

    /* Allocate an array of 'OMX_BUFFERHEADERTYPE *' */
    pp_bufs = (OMX_BUFFERHEADERTYPE **)
              malloc(count * sizeof(OMX_BUFFERHEADERTYPE *));

    for (index = 0; index < count; index++)
    {
        /* See 'Table 6-3' in document 'R01USxxxxEJxxxx_vecmn_v1.0.pdf' */
        if (p_bufs[index].size != port.nBufferSize)
        {
            printf("Error: Require size '%d', not '%ld'\n",
                   port.nBufferSize, p_bufs[index].size);
            break;
        }

        /* See section 2.2.9 in document 'R01USxxxxEJxxxx_cmn_v1.0.pdf'
         * and section 4.1.1 in document 'R01USxxxxEJxxxx_vecmn_v1.0.pdf' */
        if (OMX_ErrorNone != OMX_UseBuffer(handle, pp_bufs + index,
                                           port_idx, NULL, p_bufs[index].size,
                                           (OMX_U8 *)(p_bufs[index].hard_addr)))
        {
            printf("Error: Failed to use buffer at index '%d'\n", index);
            break;
        }
    }

    if (index < count)
    {
        omx_dealloc_port_bufs(handle, port_idx, pp_bufs, index);
        return NULL;
    }

    return pp_bufs;
}

OMX_BUFFERHEADERTYPE ** omx_alloc_buffers(OMX_HANDLETYPE handle,
                                          OMX_U32 port_idx)
{
    uint32_t index = 0;

    OMX_PARAM_PORTDEFINITIONTYPE port;
    OMX_BUFFERHEADERTYPE ** pp_bufs = NULL;

    /* Get port */
    if (omx_get_port(handle, port_idx, &port) == false)
    {
        return NULL;
    }

    /* Allocate an array of 'OMX_BUFFERHEADERTYPE *' */
    pp_bufs = (OMX_BUFFERHEADERTYPE **)
              malloc(port.nBufferCountActual * sizeof(OMX_BUFFERHEADERTYPE *));


    for (index = 0; index < port.nBufferCountActual; index++)
    {
        /* See section 2.2.10 in document 'R01USxxxxEJxxxx_cmn_v1.0.pdf'
         * and 'Table 6-3' in document 'R01USxxxxEJxxxx_vecmn_v1.0.pdf' */
        if (OMX_ErrorNone != OMX_AllocateBuffer(handle,
                                                pp_bufs + index,
                                                port_idx, NULL,
                                                port.nBufferSize))
        {
            printf("Error: Failed to allocate buffers at index '%d'\n", index);
            break;
        }
    }

    if (index < port.nBufferCountActual)
    {
        omx_dealloc_port_bufs(handle, port_idx, pp_bufs, index);
        return NULL;
    }

    return pp_bufs;
}

void omx_dealloc_port_bufs(OMX_HANDLETYPE handle, OMX_U32 port_idx,
                           OMX_BUFFERHEADERTYPE ** pp_bufs, uint32_t count)
{
    uint32_t index = 0;

    /* Check parameter */
    assert(pp_bufs != NULL);

    for (index = 0; index < count; index++)
    {
        OMX_FreeBuffer(handle, port_idx, pp_bufs[index]);
    }

    /* Free entire array */
    free (pp_bufs);
}

void omx_dealloc_all_port_bufs(OMX_HANDLETYPE handle, OMX_U32 port_idx,
                               OMX_BUFFERHEADERTYPE ** pp_bufs)
{
    OMX_PARAM_PORTDEFINITIONTYPE port;

    /* Check parameter */
    assert(pp_bufs != NULL);

    /* Get port */
    if (omx_get_port(handle, port_idx, &port) == true)
    {
        omx_dealloc_port_bufs(handle, port_idx, pp_bufs,
                              port.nBufferCountActual);
    }
}

int omx_get_index(OMX_BUFFERHEADERTYPE * p_buf,
                  OMX_BUFFERHEADERTYPE ** pp_bufs, uint32_t count)
{
    int ret = -1;
    uint32_t index = 0;

    /* Check parameters */
    assert(p_buf != NULL);
    assert((pp_bufs != NULL) && (count > 0));

    for (index = 0; index < count; index++)
    {
        if (pp_bufs[index] == p_buf)
        {
            ret = index;
            break;
        }
    }

    return ret;
}

bool omx_fill_buffers(OMX_HANDLETYPE handle,
                      OMX_BUFFERHEADERTYPE ** pp_bufs, uint32_t count)
{
    bool is_success = true;
    uint32_t index = 0;

    /* Check parameters */
    assert((pp_bufs != NULL) && (count > 0));

    for (index = 0; index < count; index++)
    {
        if (OMX_FillThisBuffer(handle, pp_bufs[index]) != OMX_ErrorNone)
        {
            printf("Error: Failed to send buffer '%d' to output port\n", index);

            is_success = false;
            break;
        }
    }

    return is_success;
}

/********************************** FOR EGL ***********************************/

EGLDisplay egl_create_display()
{
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLContext context = EGL_NO_CONTEXT;

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

    EGLConfig fb_config;
    EGLint config_cnt = 0;

    /* Get default EGL display connection */
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY)
    {
        printf("Error: Failed to get EGL display\n");
        return EGL_NO_DISPLAY;
    }

    /* Initialize the EGL display connection */
    if (eglInitialize(display, NULL, NULL) == EGL_FALSE)
    {
        printf("Error: Failed to initialize EGL display\n");
        return EGL_NO_DISPLAY;
    }

    /* Get a list of EGL frame buffer configurations */
    eglChooseConfig(display, config_attribs, &fb_config, 1, &config_cnt);
    if (config_cnt == 0)
    {
        printf("Error: Failed to get EGL frame buffer configs\n");
        return EGL_NO_DISPLAY;
    }

    /* Create an EGL rendering context for the current rendering API.
     * The context can then be used to render into an EGL drawing surface */
    context = eglCreateContext(display, fb_config,
                               EGL_NO_CONTEXT, context_attribs);
    if (context == EGL_NO_CONTEXT)
    {
        printf("Error: Failed to create EGL context\n");
        return EGL_NO_DISPLAY;
    }

    /* Bind context without read and draw surfaces */
    if (EGL_FALSE ==
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context))
    {
        printf("Error: Failed to bind context without surfaces\n");
        return EGL_NO_DISPLAY;
    }

    return display;
}

void egl_delete_display(EGLDisplay display)
{
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglTerminate(display);
}

bool egl_is_ext_supported(EGLDisplay display, const char * p_name)
{
    const char * p_ext_funcs = NULL;

    /* Check parameters */
    assert((display != EGL_NO_DISPLAY) && (p_name != NULL));

    /* A non-NULL return value from 'eglGetProcAddress' does not
     * guarantee that an extension function is supported at runtime.
     *
     * For EGL extension functions, the program must also make
     * a corresponding query 'eglQueryString' to determine if a
     * function is supported by EGL */
    p_ext_funcs = eglQueryString(display, EGL_EXTENSIONS);
    if (p_ext_funcs == NULL)
    {
        printf("Error: Failed to get EGL extensions\n");
        return false;
    }

    /* Find extension 'p_name' in 'p_ext_funcs' */
    if (str_find_whole_str(p_ext_funcs, " ", p_name) == false)
    {
        printf("Error: Extension '%s' does not exist\n", p_name);
        return false;
    }

    return true;
}

bool egl_init_ext_funcs(EGLDisplay display)
{
    /* Check parameter */
    assert(display != EGL_NO_DISPLAY);

    /* Get address of function 'eglCreateImageKHR' */
    eglCreateImageKHR = (EGLCREATEIMAGEKHR)
                        eglGetProcAddress("eglCreateImageKHR");

    /* Get address of function 'eglDestroyImageKHR' */
    eglDestroyImageKHR = (EGLDESTROYIMAGEKHR)
                         eglGetProcAddress("eglDestroyImageKHR");

    if ((eglCreateImageKHR == NULL) || (eglDestroyImageKHR == NULL) ||
        !egl_is_ext_supported(display, "EGL_KHR_image_base") ||
        !egl_is_ext_supported(display, "EGL_EXT_image_dma_buf_import"))
    {
        printf("Error: Failed to init EGL extension functions\n");
        return false;
    }

    return true;
}

EGLImageKHR egl_create_yuyv_image(EGLDisplay display, uint32_t img_width,
                                  uint32_t img_height, int dmabuf_fd)
{
    EGLImageKHR img = EGL_NO_IMAGE_KHR;

    /* Check parameters */
    assert(display != EGL_NO_DISPLAY);
    assert((img_width > 0) && (img_height > 0) && (dmabuf_fd > 0));

    EGLint img_attribs[] =
    {
        /* The logical dimensions of YUYV buffer in pixels */
        EGL_WIDTH, img_width,
        EGL_HEIGHT, img_height,

        /* Pixel format of the buffer, as specified by 'drm_fourcc.h' */
        EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_YUYV,

        /* The dmabuf file descriptor of plane 0 of the image */
        EGL_DMA_BUF_PLANE0_FD_EXT, dmabuf_fd,

        /* The offset from the start of the dmabuf of the first sample in
         * plane 0, in bytes */
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,

        /* The number of bytes between the start of subsequent rows of samples
         * in plane 0 */
        EGL_DMA_BUF_PLANE0_PITCH_EXT, img_width * 2, /* 2 bytes per pixel */

        /* Y, U, and V color range from [0, 255] */
        EGL_SAMPLE_RANGE_HINT_EXT, EGL_YUV_FULL_RANGE_EXT,

        /* The chroma samples are sub-sampled only in horizontal dimension,
         * by a factor of 2 */
        EGL_YUV_CHROMA_VERTICAL_SITING_HINT_EXT, EGL_YUV_CHROMA_SITING_0_EXT,
        EGL_YUV_CHROMA_HORIZONTAL_SITING_HINT_EXT,
                                                EGL_YUV_CHROMA_SITING_0_5_EXT,
        EGL_NONE,
    };

    /* Create EGLImage from a Linux dmabuf file descriptor */
    img = eglCreateImageKHR(display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT,
                            (EGLClientBuffer)NULL, img_attribs);
    if (img == EGL_NO_IMAGE_KHR)
    {
        printf("Error: Failed to create YUYV EGLImage\n");
    }

    return img;
}

EGLImageKHR * egl_create_yuyv_images(EGLDisplay display,
                                     uint32_t img_width, uint32_t img_height,
                                     v4l2_dmabuf_exp_t * p_bufs, uint32_t cnt)
{
    EGLImageKHR * p_imgs = NULL;
    uint32_t index = 0;

    /* Check parameters */
    assert(display != EGL_NO_DISPLAY);
    assert((img_width > 0) && (img_height > 0));
    assert((p_bufs != NULL) && (cnt > 0));

    p_imgs = (EGLImageKHR *)malloc(cnt * sizeof(EGLImageKHR));

    for (index = 0; index < cnt; index++)
    {
        p_imgs[index] = egl_create_yuyv_image(display,
                                              img_width, img_height,
                                              p_bufs[index].dmabuf_fd);
        if (p_imgs[index] == EGL_NO_IMAGE_KHR)
        {
            break;
        }
    }

    if (index < cnt)
    {
        egl_delete_images(display, p_imgs, index);
        return NULL;
    }

    return p_imgs;
}

EGLImageKHR egl_create_nv12_image(EGLDisplay display,
                                  uint32_t img_width, uint32_t img_height,
                                  int y_dmabuf_fd, int uv_dmabuf_fd)
{
    EGLImageKHR img = EGL_NO_IMAGE_KHR;

    /* Check parameters */
    assert(display != EGL_NO_DISPLAY);
    assert((img_width > 0) && (img_height > 0));
    assert((y_dmabuf_fd > 0) && (uv_dmabuf_fd > 0));

    EGLint img_attribs[] =
    {
        /* The logical dimensions of NV12 buffer in pixels */
        EGL_WIDTH, img_width,
        EGL_HEIGHT, img_height,

        /* Pixel format of the buffer, as specified by 'drm_fourcc.h' */
        EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_NV12,

        EGL_DMA_BUF_PLANE0_FD_EXT, y_dmabuf_fd,
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
        EGL_DMA_BUF_PLANE0_PITCH_EXT, img_width,

        /* These attribute-value pairs are necessary because NV12 contains 2
         * planes: plane 0 is for Y values and plane 1 is for chroma values */
        EGL_DMA_BUF_PLANE1_FD_EXT, uv_dmabuf_fd,
        EGL_DMA_BUF_PLANE1_OFFSET_EXT, 0,
        EGL_DMA_BUF_PLANE1_PITCH_EXT, img_width,

        /* Y, U, and V color range from [0, 255] */
        EGL_SAMPLE_RANGE_HINT_EXT, EGL_YUV_FULL_RANGE_EXT,

        /* The chroma plane are sub-sampled in both horizontal and vertical
         * dimensions, by a factor of 2 */
        EGL_YUV_CHROMA_VERTICAL_SITING_HINT_EXT, EGL_YUV_CHROMA_SITING_0_5_EXT,
        EGL_YUV_CHROMA_HORIZONTAL_SITING_HINT_EXT,
                                                EGL_YUV_CHROMA_SITING_0_5_EXT,
        EGL_NONE,
    };

    /* Create EGLImage from a Linux dmabuf file descriptor */
    img = eglCreateImageKHR(display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT,
                            (EGLClientBuffer)NULL, img_attribs);
    if (img == EGL_NO_IMAGE_KHR)
    {
        printf("Error: Failed to create NV12 EGLImage\n");
    }

    return img;
}

EGLImageKHR * egl_create_nv12_images(EGLDisplay display,
                                     uint32_t img_width, uint32_t img_height,
                                     mmngr_buf_t * p_bufs, uint32_t count)
{
    EGLImageKHR * p_imgs = NULL;

    uint32_t index = 0;
    mmngr_dmabuf_exp_t * p_dmabufs = NULL;

    /* Check parameters */
    assert(display != EGL_NO_DISPLAY);
    assert((img_width > 0) && (img_height > 0));
    assert((p_bufs != NULL) && (count > 0));

    p_imgs = (EGLImageKHR *)malloc(count * sizeof(EGLImageKHR));

    for (index = 0; index < count; index++)
    {
        p_dmabufs = p_bufs[index].p_dmabufs;

        p_imgs[index] = egl_create_nv12_image(display,
                                              img_width, img_height,
                                              p_dmabufs[0].dmabuf_fd,
                                              p_dmabufs[1].dmabuf_fd);
        if (p_imgs[index] == EGL_NO_IMAGE_KHR)
        {
            break;
        }
    }

    if (index < count)
    {
        egl_delete_images(display, p_imgs, index);
        return NULL;
    }

    return p_imgs;
}

void egl_delete_images(EGLDisplay display,
                       EGLImageKHR * p_imgs, uint32_t count)
{
    uint32_t index = 0;

    /* Check parameters */
    assert((display != EGL_NO_DISPLAY) && (p_imgs != NULL));

    for (index = 0; index < count; index++)
    {
        eglDestroyImageKHR(display, p_imgs[index]);
    }

    /* Free entire array */
    free(p_imgs);
}

/******************************* FOR OPENGL ES ********************************/

GLuint gl_create_shader(const char * p_file, GLenum type)
{
    GLuint shader = 0;
    char * p_shader_src = NULL;

    GLint log_len = 0;
    char * p_log  = NULL;

    GLint b_compile_ok = GL_FALSE;

    /* Check parameter */
    assert(p_file != NULL);

    p_shader_src = file_read_str(p_file);
    if (p_shader_src == NULL)
    {
        printf("Error: Failed to open '%s'\n", p_file);
        return 0;
    }

    /* Create an empty shader object */
    shader = glCreateShader(type);
    if (shader == 0)
    {
        printf("Error: Failed to create shader object\n");
        return 0;
    }

    /* Copy source code into the shader object */
    glShaderSource(shader, 1, (const GLchar **)&p_shader_src, NULL);
    free(p_shader_src);

    /* Compile the source code strings that was stored in the shader object */
    glCompileShader(shader);

    /* Get status of the last compile operation on the shader object */
    glGetShaderiv(shader, GL_COMPILE_STATUS, &b_compile_ok);
    if (b_compile_ok == GL_FALSE)
    {
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_len);
        p_log = (char *)malloc(log_len);

        /* Get the compile error of the shader object */
        glGetShaderInfoLog(shader, log_len, NULL, p_log);
        printf("Error: Failed to compile shader:\n%s\n", p_log);
        free(p_log);

        glDeleteShader(shader);
        return 0;
    }

  return shader;
}

GLuint gl_create_prog_from_objs(GLuint vs_object, GLuint fs_object)
{
    GLuint program = 0;

    GLint log_len = 0;
    char * p_log  = NULL;

    GLint b_link_ok = GL_FALSE;

    /* Check parameters */
    assert((vs_object != 0) && (fs_object != 0));

    /* Create an empty program object */
    program = glCreateProgram();
    if (program == 0)
    {
        printf("Error: Failed to create program object\n");
        return 0;
    }

    /* Attach the shader object to the program object */
    glAttachShader(program, vs_object);
    glAttachShader(program, fs_object);

    /* Link the program object */
    glLinkProgram(program);

    /* Get status of the last link operation on program object */
    glGetProgramiv(program, GL_LINK_STATUS, &b_link_ok);
    if (b_link_ok == GL_FALSE)
    {
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_len);
        p_log = (char *)malloc(log_len);

        /* Get the link error of the program object */
        glGetProgramInfoLog(program, log_len, NULL, p_log);
        printf("Error: Failed to link program:\n%s\n", p_log);
        free(p_log);

        glDeleteProgram(program);
        return 0;
    }

    return program;
}

GLuint gl_create_prog_from_src(const char * p_vs_file, const char * p_fs_file)
{
    GLuint vs_object   = 0;
    GLuint fs_object   = 0;
    GLuint prog_object = 0;

    /* Check parameters */
    assert((p_vs_file != NULL) && (p_fs_file != NULL));

    /* Create and compile vertex shader object */
    vs_object = gl_create_shader(p_vs_file, GL_VERTEX_SHADER);

    /* Create and compile fragment shader object */
    fs_object = gl_create_shader(p_fs_file, GL_FRAGMENT_SHADER);

    /* Create program object and
     * link vertex shader object and fragment shader object to it */
    prog_object = gl_create_prog_from_objs(vs_object, fs_object);

    /* The vertex shader object and fragment shader object are not needed
     * after creating program. So, it should be deleted */
    glDeleteShader(vs_object);
    glDeleteShader(fs_object);

    return prog_object;
}

bool gl_is_ext_supported(const char * p_name)
{
    const char * p_ext_funcs = NULL;

    /* Check parameter */
    assert(p_name != NULL);

    /* A non-NULL return value from 'eglGetProcAddress' does not
     * guarantee that an extension function is supported at runtime.
     *
     * For OpenGL ES extension functions, the program must also make
     * a corresponding query 'glGetString' to determine if a function is
     * supported by a specific client API context */
    p_ext_funcs = (const char *)glGetString(GL_EXTENSIONS);
    if (p_ext_funcs == NULL)
    {
        printf("Error: Failed to get OpenGL ES extensions\n");
        return false;
    }

    /* Find extension 'p_name' in 'p_ext_funcs' */
    if (str_find_whole_str(p_ext_funcs, " ", p_name) == false)
    {
        printf("Error: Extension '%s' does not exist\n", p_name);
        return false;
    }

    return true;
}

bool gl_init_ext_funcs()
{
    /* Get address of function 'glEGLImageTargetTexture2DOES' */
    glEGLImageTargetTexture2DOES = (GLEGLIMAGETARGETTEXTURE2DOES)
                            eglGetProcAddress("glEGLImageTargetTexture2DOES");

    if ((glEGLImageTargetTexture2DOES == NULL) ||
        !gl_is_ext_supported("GL_OES_EGL_image_external") ||
        !gl_is_ext_supported("GL_OES_EGL_image_external_essl3") ||
        !gl_is_ext_supported("GL_EXT_YUV_target"))
    {
        printf("Error: Failed to init OpenGL ES extension functions\n");
        return false;
    }

    return true;
}

GLuint gl_create_texture(EGLImageKHR image)
{
    GLuint texture = 0;

    /* Check parameter */
    assert(image != EGL_NO_IMAGE_KHR);

    /* Use external texture for special image layouts, such as: YUYV, NV12... */
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture);

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
    glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, image);

    /* Unbind texture */
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);

    return texture;
}

GLuint * gl_create_textures(EGLImageKHR * p_images, uint32_t count)
{
    GLuint * p_textures = NULL;
    uint32_t index = 0;

    /* Check parameters */
    assert((p_images != NULL) && (count > 0));

    p_textures = (GLuint *)malloc(count * sizeof(GLuint));

    for (index = 0; index < count; index++)
    {
        p_textures[index] = gl_create_texture(p_images[index]);
        if (p_textures[index] == 0)
        {
            printf("Error: Failed to create texture at index '%d'\n", index);
            break;
        }
    }

    if (index < count)
    {
        gl_delete_textures(p_textures, index);
        return NULL;
    }

    return p_textures;
}

void gl_delete_textures(GLuint * p_textures, uint32_t count)
{
    /* Check parameter */
    assert(p_textures != NULL);

    if (count > 0)
    {
        glDeleteTextures(count, p_textures);
    }

    /* Free entire array */
    free(p_textures);
}

GLuint gl_create_framebuffer(GLuint texture)
{
    GLuint fb = 0;

    /* Check parameter */
    assert(texture != 0);

    /* Create and bind framebuffer */
    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);

    /* Attach texture to the color buffer of currently bound framebuffer */
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_EXTERNAL_OES, texture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        printf("Error: Failed to attach texture to framebuffer\n");
        glDeleteFramebuffers(1, &fb);

        return 0;
    }

    /* Unbind framebuffer */
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return fb;
}

GLuint * gl_create_framebuffers(GLuint * p_textures, uint32_t count)
{
    GLuint * p_fbs = NULL;
    uint32_t index = 0;

    /* Check parameters */
    assert((p_textures != NULL) && (count > 0));

    p_fbs = (GLuint *)malloc(count * sizeof(GLuint));

    for (index = 0; index < count; index++)
    {
        p_fbs[index] = gl_create_framebuffer(p_textures[index]);
        if (p_fbs[index] == 0)
        {
            break;
        }
    }

    if (index < count)
    {
        gl_delete_framebuffers(p_fbs, index);
        return NULL;
    }

    return p_fbs;
}

void gl_delete_framebuffers(GLuint * p_fbs, uint32_t count)
{
    /* Check parameter */
    assert(p_fbs != NULL);

    if (count > 0)
    {
        glDeleteFramebuffers(count, p_fbs);
    }

    /* Free entire array */
    free(p_fbs);
}

gl_resources_t gl_create_resources()
{
    gl_resources_t res;

    /* For drawing rectangle */

    /* The positions and colors of rectangle */
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

    /* For YUYV-to-NV12 conversion */

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

    /* Create program object for rendering rectangle */
    res.shape_prog = gl_create_prog_from_src("shape-rendering.vs.glsl",
                                             "shape-rendering.fs.glsl");

    /* Create program object for converting YUYV to NV12 */
    res.conv_prog = gl_create_prog_from_src("yuyv-to-nv12-conv.vs.glsl",
                                            "yuyv-to-nv12-conv.fs.glsl");

    /* Get variable 'yuyvTexture' from fragment shader */
    res.uniform_yuyv_texture = glGetUniformLocation(res.conv_prog,
                                                    "yuyvTexture");

    /* Create vertex/index buffer objects and add data to it */
    glGenBuffers(1, &(res.vbo_rec_vertices));
    glBindBuffer(GL_ARRAY_BUFFER, res.vbo_rec_vertices);
    glBufferData(GL_ARRAY_BUFFER, sizeof(rec_vertices), rec_vertices,
                 GL_STATIC_DRAW);

    glGenBuffers(1, &(res.ibo_rec_indices));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, res.ibo_rec_indices);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(rec_indices), rec_indices,
                 GL_STATIC_DRAW);

    glGenBuffers(1, &(res.vbo_canvas_vertices));
    glBindBuffer(GL_ARRAY_BUFFER, res.vbo_canvas_vertices);
    glBufferData(GL_ARRAY_BUFFER, sizeof(canvas_vertices), canvas_vertices,
                 GL_STATIC_DRAW);

    glGenBuffers(1, &(res.ibo_canvas_indices));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, res.ibo_canvas_indices);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(canvas_indices),
                 canvas_indices, GL_STATIC_DRAW);

    /* Unbind buffers */
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    return res;
}

void gl_delete_resources(gl_resources_t res)
{
    /* Delete vertex buffer objects */
    glDeleteBuffers(1, &(res.vbo_rec_vertices));
    glDeleteBuffers(1, &(res.vbo_canvas_vertices));

    /* Delete index buffer objects */
    glDeleteBuffers(1, &(res.ibo_rec_indices));
    glDeleteBuffers(1, &(res.ibo_canvas_indices));

    /* Delete program objects */
    glDeleteProgram(res.shape_prog);
    glDeleteProgram(res.conv_prog);
}

void gl_draw_rectangle(gl_resources_t res)
{
    GLint tmp_size = 0;

    /* Use program object for drawing rectangle */
    glUseProgram(res.shape_prog);

    /* Enable attribute 'aPos' since it's disabled by default */
    glEnableVertexAttribArray(0);

    /* Enable attribute 'aColor' since it's disabled by default */
    glEnableVertexAttribArray(1);

    /* Show OpenGL ES how the 'vbo_rec_vertices' should be interpreted */
    glBindBuffer(GL_ARRAY_BUFFER, res.vbo_rec_vertices);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                          5 * sizeof(GLfloat), (void *)0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                          5 * sizeof(GLfloat), (void *)(2 * sizeof(GLfloat)));

    /* Draw rectangle */
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, res.ibo_rec_indices);
    glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &tmp_size);

    glDrawElements(GL_TRIANGLES,
                   tmp_size / sizeof(GLushort), GL_UNSIGNED_SHORT, 0);

    /* Wait until 'glDrawElements' finishes */
    glFinish();

    /* Unbind buffers */
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    /* Disable attributes */
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
}

void gl_conv_yuyv_to_nv12(GLuint yuyv_texture, gl_resources_t res)
{
    GLint tmp_size = 0;

    /* Check parameter */
    assert(yuyv_texture != 0);

    /* Use program object for YUYV-to-NV12 conversion */
    glUseProgram(res.conv_prog);

    /* Enable attribute 'aPos' since it's disabled by default */
    glEnableVertexAttribArray(0);

    /* Enable attribute 'aYuyvTexCoord' since it's disabled by default */
    glEnableVertexAttribArray(1);

    /* Show OpenGL ES how the 'vbo_canvas_vertices' should be interpreted */
    glBindBuffer(GL_ARRAY_BUFFER, res.vbo_canvas_vertices);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(GLfloat), (void*)0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));

    /* Bind the YUYV texture to texture unit 0 */
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, yuyv_texture);

    /* Tell OpenGL sampler 'yuyvTexture' belongs to texture unit 0 */
    glUniform1i(res.uniform_yuyv_texture, /*GL_TEXTURE*/0);

    /* Convert YUYV texture to NV12 texture */
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, res.ibo_canvas_indices);
    glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &tmp_size);

    glDrawElements(GL_TRIANGLES,
                   tmp_size / sizeof(GLushort), GL_UNSIGNED_SHORT, 0);

    /* Wait until 'glDrawElements' finishes */
    glFinish();

    /* Unbind texture */
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);

    /* Unbind buffers */
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    /* Disable attributes */
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
}

/********************************* FOR MMNGR **********************************/

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
    plane1_size = page_size_get_aligned_size((1.0f * nv12_size) / 3);

    /* Exit if size of plane 0 is not aligned to page size */
    if (!page_size_is_size_aligned(plane0_size))
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

/******************************** FOR THREADS *********************************/

void * thread_input(void * p_param)
{
    in_data_t * p_data = (in_data_t *)p_param;

    /* true:  The loop is running.
     * false: The loop just stopped */
    bool is_running = true;

    /* V4L2 buffer */
    struct v4l2_buffer cam_buf;

    /* Buffer of input port */
    int index = -1;
    OMX_BUFFERHEADERTYPE * p_buf = NULL;

    /* EGL display connection */
    EGLDisplay display = EGL_NO_DISPLAY;

    /* Resources for OpenGL ES */
    gl_resources_t gl_resources;

    /* YUYV images and textures */
    EGLImageKHR * p_yuyv_imgs = NULL;
    GLuint      * p_yuyv_texs = NULL;

    /* NV12 images, textures, and framebuffers */
    EGLImageKHR * p_nv12_imgs = NULL;
    GLuint      * p_nv12_texs = NULL;
    GLuint      * p_nv12_fbs  = NULL;

    /* Check parameter */
    assert(p_data != NULL);

    /**************************************************************************
     *                    STEP 1: SET UP EGL AND OPENGL ES                    *
     **************************************************************************/

    /* Create EGL display */
    display = egl_create_display();
    assert(display != EGL_NO_DISPLAY);

    /* Initialize OpenGL ES and EGL extension functions */
    assert(gl_init_ext_funcs());
    assert(egl_init_ext_funcs(display));

    /* Create resources needed for rendering */
    gl_resources = gl_create_resources();

    /* Make sure Viewport matches the width and height of YUYV buffer */
    glViewport(0, 0, FRAME_WIDTH_IN_PIXELS, FRAME_HEIGHT_IN_PIXELS);

    /**************************************************************************
     *               STEP 2: CREATE TEXTURES FROM YUYV BUFFERS                *
     **************************************************************************/

    /* Exit program if size of YUYV buffer is not aligned to page size.
     *
     * Mali library requires that both address and size of dmabuf must be
     * multiples of page size.
     *
     * With dimension 640x480, size of plane 1 (UV plane) of NV12 buffer
     * is 153,600 bytes.
     * Since this size is not a multiple of page size, the Mali library will
     * output the following messages when dmabuf of plane 1 is imported to it:
     *
     *  [   28.144983] sg_dma_len(s)=153600 is not a multiple of PAGE_SIZE
     *  [   28.151050] WARNING: CPU: 1 PID: 273 at mali_kbase_mem_linux.c:1184
     *                 kbase_mem_umm_map_attachment+0x1a8/0x270 [mali_kbase]
     *  ... */
    assert(page_size_is_size_aligned(YUYV_FRAME_SIZE_IN_BYTES));

    /* Create YUYV EGLImage objects */
    p_yuyv_imgs = egl_create_yuyv_images(display,
                                         FRAME_WIDTH_IN_PIXELS,
                                         FRAME_HEIGHT_IN_PIXELS,
                                         p_data->p_yuyv_bufs,
                                         YUYV_BUFFER_COUNT);
    assert(p_yuyv_imgs != NULL);

    /* Create YUYV textures */
    p_yuyv_texs = gl_create_textures(p_yuyv_imgs, YUYV_BUFFER_COUNT);
    assert(p_yuyv_texs != NULL);

    /**************************************************************************
     *               STEP 3: CREATE TEXTURES FROM NV12 BUFFERS                *
     **************************************************************************/

    /* Create NV12 EGLImage objects */
    p_nv12_imgs = egl_create_nv12_images(display,
                                         FRAME_WIDTH_IN_PIXELS,
                                         FRAME_HEIGHT_IN_PIXELS,
                                         p_data->p_nv12_bufs,
                                         NV12_BUFFER_COUNT);
    assert(p_nv12_imgs != NULL);

    /* Create NV12 textures */
    p_nv12_texs = gl_create_textures(p_nv12_imgs, NV12_BUFFER_COUNT);
    assert(p_nv12_texs != NULL);

    /**************************************************************************
     *             STEP 4: CREATE FRAMEBUFFERS FROM NV12 TEXTURES             *
     **************************************************************************/

    /* Create framebuffers */
    p_nv12_fbs = gl_create_framebuffers(p_nv12_texs, NV12_BUFFER_COUNT);
    assert(p_nv12_fbs != NULL);

    /**************************************************************************
     *                       STEP 5: THREAD'S MAIN LOOP                       *
     **************************************************************************/

    while (is_running)
    {
        assert(pthread_mutex_lock(p_data->p_mutex) == 0);

        while (queue_is_empty(p_data->p_queue))
        {
            /* Thread will sleep until the queue is not empty */
            assert(0 == pthread_cond_wait(p_data->p_cond_available,
                                          p_data->p_mutex));
        }

        /* At this point, the queue must have something in it */
        assert(!queue_is_empty(p_data->p_queue));

        /* Receive buffer (of input port) from the queue */
        p_buf = *(OMX_BUFFERHEADERTYPE **)(queue_dequeue(p_data->p_queue));
        assert(p_buf != NULL);

        assert(pthread_mutex_unlock(p_data->p_mutex) == 0);

        /* Get buffer's index */
        index = omx_get_index(p_buf, p_data->pp_bufs, NV12_BUFFER_COUNT);
        assert(index != -1);

        /* Receive camera's buffer */
        assert(v4l2_dequeue_buf(p_data->cam_fd, &cam_buf));

        /* Bind framebuffer.
         * All subsequent rendering operations will now render to NV12 texture
         * which is linked to the framebuffer (see above):
         * https://learnopengl.com/Advanced-OpenGL/Framebuffers */
        glBindFramebuffer(GL_FRAMEBUFFER, p_nv12_fbs[index]);

        /* Convert YUYV texture to NV12 texture */
        gl_conv_yuyv_to_nv12(p_yuyv_texs[cam_buf.index], gl_resources);

        /* Draw rectangle on NV12 texture */
        gl_draw_rectangle(gl_resources);

        /* Reuse camera's buffer */
        assert(v4l2_enqueue_buf(p_data->cam_fd, cam_buf.index));

        /* If 'p_buf' contains data, 'nFilledLen' must not be zero */
        p_buf->nFilledLen = NV12_FRAME_SIZE_IN_BYTES;

        /* Section 6.14.1 in document 'R01USxxxxEJxxxx_vecmn_v1.0.pdf' */
        p_buf->nFlags = OMX_BUFFERFLAG_ENDOFFRAME;

        if (g_int_signal)
        {
            p_buf->nFlags |= OMX_BUFFERFLAG_EOS;

            /* Exit loop */
            is_running = false;
        }

        /* Send the buffer to the input port of the component */
        assert(OMX_EmptyThisBuffer(p_data->handle, p_buf) == OMX_ErrorNone);
    }

    /**************************************************************************
     *                 STEP 6: CLEAN UP OPENGL ES'S RESOURCES                 *
     **************************************************************************/

    /* Delete framebuffers and NV12 textures */
    gl_delete_framebuffers(p_nv12_fbs, NV12_BUFFER_COUNT);

    gl_delete_textures(p_nv12_texs, NV12_BUFFER_COUNT);
    egl_delete_images(display, p_nv12_imgs, NV12_BUFFER_COUNT);

    /* Delete YUYV textures */
    gl_delete_textures(p_yuyv_texs, YUYV_BUFFER_COUNT);
    egl_delete_images(display, p_yuyv_imgs, YUYV_BUFFER_COUNT);

    /* Delete resources for OpenGL ES */
    gl_delete_resources(gl_resources);

    /* Delete EGL display */
    egl_delete_display(display);

    printf("Thread '%s' exited\n", __FUNCTION__);
    return NULL;
}

void * thread_output(void * p_param)
{
    out_data_t * p_data = (out_data_t *)p_param;

    /* true:  The loop is running.
     * false: The loop just stopped */
    bool is_running = true;

    /* Buffer of output port */
    OMX_BUFFERHEADERTYPE * p_buf = NULL;

    /* File for writing H.264 data */
    FILE * p_h264_fd = NULL;

    /* Check parameter */
    assert(p_data != NULL);

    /* Open file */
    p_h264_fd = fopen(H264_FILE_NAME, "w");
    assert(p_h264_fd != NULL);

    while (is_running)
    {
        assert(pthread_mutex_lock(p_data->p_mutex) == 0);

        while (queue_is_empty(p_data->p_queue))
        {
            /* The thread will sleep until the queue is not empty */
            assert(0 == pthread_cond_wait(p_data->p_cond_available,
                                          p_data->p_mutex));
        }

        /* At this point, the queue must have something in it */
        assert(!queue_is_empty(p_data->p_queue));

        /* Receive buffer from the queue */
        p_buf = *(OMX_BUFFERHEADERTYPE **)(queue_dequeue(p_data->p_queue));
        assert(p_buf != NULL);

        assert(pthread_mutex_unlock(p_data->p_mutex) == 0);

        /* Write H.264 data to a file */
        fwrite((char *)(p_buf->pBuffer), 1, p_buf->nFilledLen, p_h264_fd);

        if (p_buf->nFlags & OMX_BUFFERFLAG_EOS)
        {
            /* Exit loop */
            is_running = false;
        }
        else
        {
            p_buf->nFilledLen = 0;
            p_buf->nFlags     = 0;

            /* Send the buffer to the output port of the component */
            assert(OMX_FillThisBuffer(p_data->handle, p_buf) == OMX_ErrorNone);
        }
    }

    /* Close file */
    fclose(p_h264_fd);

    printf("Thread '%s' exited\n", __FUNCTION__);
    return NULL;
}

/****************************** FOR FILE ACCESS *******************************/

char * file_read_str(const char * p_name)
{
    long size = -1;
    char * p_content = NULL;

    /* Check parameter */
    assert(p_name != NULL);

    /* Open file */
    FILE * p_fd = fopen(p_name, "r");
    if (p_fd != NULL)
    {
        /* Get file's size */
        fseek(p_fd, 0, SEEK_END);
        size = ftell(p_fd);
        rewind(p_fd);

        /* Prepare buffer to store file's content */
        p_content = (char *)malloc(size + 1);
        memset(p_content, 0, size + 1);

        /* Read file's content and put it into buffer */
        fread(p_content, 1, size, p_fd);
        p_content[size] = '\0';

        /* Close file */
        fclose(p_fd);
    }

    return p_content;
}

void file_write_buffer(const char * p_name, const char * p_buffer, size_t size)
{
    /* Check parameters */
    assert((p_name != NULL) && (p_buffer != NULL) && (size > 0));

    /* Open file */
    FILE * p_fd = fopen(p_name, "w");
    if (p_fd != NULL)
    {
        /* Write content to the file */
        fwrite(p_buffer, 1, size, p_fd);

        /* Close file */
        fclose(p_fd);
    }
}

/***************************** FOR PAGE ALIGNMENT *****************************/

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

/***************************** FOR STRING SEARCH ******************************/

char * str_to_uppercase(char * p_str)
{
    int index = 0;

    /* Check parameter */
    assert(p_str != NULL);

    for (index = 0; p_str[index] != '\0'; index++)
    {
        /* If 'p_str[index]' is neither an 'unsigned char' value or 'EOF',
         * the behavior of function 'toupper' is undefined.
         *
         * Therefore, 'p_str[index]' must be cast to 'unsigned char' */
        p_str[index] = (char)toupper((unsigned char)p_str[index]);
    }

    return p_str;
}

bool str_find_whole_str(const char * p_str_arr,
                        const char * p_delim_str, const char * p_str)
{
    bool b_is_found = false;

    char * p_tmp_str = NULL;
    char * p_tmp_str_arr = NULL;

    char * p_save_str = NULL;
    char * p_token_str = NULL;

    /* Check parameters */
    assert((p_str_arr != NULL) && (p_delim_str != NULL) && (p_str != NULL));

    /* Copy contents of 'p_str' to 'p_tmp_str'.
     * Note: This step is required for function 'str_to_uppercase' */
    p_tmp_str = (char *)malloc(strlen(p_str) + 1);
    strcpy(p_tmp_str, p_str);

    /* Convert 'p_tmp_str' to upper case for string comparison */
    str_to_uppercase(p_tmp_str);

    /* Copy contents of 'p_str_arr' to 'p_tmp_str_arr'.
     *
     * Note: This step is required because 'strtok_r' modifies its first
     * argument. Therefore, variable 'p_str_arr' cannot be used */
    p_tmp_str_arr = (char *)malloc(strlen(p_str_arr) + 1);
    strcpy(p_tmp_str_arr, p_str_arr);

    /* Extract tokens from 'p_tmp_str_arr' */
    p_token_str = strtok_r(p_tmp_str_arr, p_delim_str, &p_save_str);

    while (p_token_str != NULL)
    {
        /* Compare each token with 'p_tmp_str' */
        if (strcmp(str_to_uppercase(p_token_str), p_tmp_str) == 0)
        {
            b_is_found = true;
            break;
        }

        p_token_str = strtok_r(NULL, p_delim_str, &p_save_str);
    }

    /* Free resources */
    free(p_tmp_str);
    free(p_tmp_str_arr);

    return b_is_found;
}

/****************************** FOR ERROR OUTPUT ******************************/

void errno_print()
{
    printf("Error: '%s' (code: '%d')\n", strerror(errno), errno);
}
