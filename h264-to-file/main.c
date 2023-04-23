#include <stdio.h>
#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>

#include "gl.h"
#include "egl.h"
#include "omx.h"
#include "util.h"
#include "v4l2.h"
#include "mmngr.h"
#include "queue.h"

/******************************************************************************
 *                              MACRO VARIABLES                               *
 ******************************************************************************/

#define FONT_FILE "LiberationSans-Regular.ttf"

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

/******************************************************************************
 *                              GLOBAL VARIABLES                              *
 ******************************************************************************/

/* 1: Interrupt signal */
volatile sig_atomic_t g_int_signal = 0;

/******************************************************************************
 *                           STRUCTURE DEFINITIONS                            *
 ******************************************************************************/

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
 *                           FUNCTION DECLARATIONS                            *
 ******************************************************************************/

/**************************** FOR SIGNAL HANDLING *****************************/

void sigint_handler(int signum, siginfo_t * p_info, void * p_ptr);

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

/******************************************************************************
 *                               MAIN FUNCTION                                *
 ******************************************************************************/

int main()
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
     *                    STEP 11: PREPARE CAPTURING DATA                     *
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
     *                          STEP 16: CLEAN UP OMX                         *
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
     *                     STEP 17: CLEAN UP V4L2 DEVICE                      *
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

void sigint_handler(int signum, siginfo_t * p_info, void * p_ptr)
{
    /* Mark parameters as unused */
    UNUSED(p_ptr);
    UNUSED(p_info);
    UNUSED(signum);

    g_int_signal = 1;
}

/********************************** FOR OMX ***********************************/

OMX_ERRORTYPE omx_event_handler(OMX_HANDLETYPE hComponent, OMX_PTR pAppData,
                                OMX_EVENTTYPE eEvent, OMX_U32 nData1,
                                OMX_U32 nData2, OMX_PTR pEventData)
{
    /* Mark parameters as unused */
    UNUSED(pAppData);
    UNUSED(hComponent);
    UNUSED(pEventData);

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
    /* Mark parameter as unused */
    UNUSED(hComponent);

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
    /* Mark parameter as unused */
    UNUSED(hComponent);

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

    /* EGL display, config, and context */
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLContext context = EGL_NO_CONTEXT;

    EGLConfig config;

    /* OpenGL ES */
    GLuint rec_prog = 0;
    GLuint text_prog = 0;
    GLuint yuyv_to_rgb_prog = 0;
    GLuint rgb_to_nv12_prog = 0;

    gl_res_t gl_res;

    /* YUYV images and textures */
    EGLImageKHR * p_yuyv_imgs = NULL;
    GLuint      * p_yuyv_texs = NULL;

    /* RGB textures and framebuffers */
    GLuint * p_rgb_texs = NULL;
    GLuint * p_rgb_fbs  = NULL;

    /* NV12 images, textures, and framebuffers */
    EGLImageKHR * p_nv12_imgs = NULL;
    GLuint      * p_nv12_texs = NULL;
    GLuint      * p_nv12_fbs  = NULL;

    /* Check parameter */
    assert(p_data != NULL);

    /**************************************************************************
     *                           STEP 1: SET UP EGL                           *
     **************************************************************************/

    /* Connect to EGL display */
    display = egl_connect_display(EGL_DEFAULT_DISPLAY, &config);
    assert(display != EGL_NO_DISPLAY);

    /* Create and bind EGL context */
    context = egl_create_context(display, config, EGL_NO_SURFACE);
    assert(context != EGL_NO_CONTEXT);

    /* Initialize EGL extension functions */
    assert(egl_init_ext_funcs(display));

    /**************************************************************************
     *                        STEP 2: SET UP OPENGL ES                        *
     **************************************************************************/

    /* Create program object for drawing rectangle */
    rec_prog = gl_create_prog_from_src("rectangle.vs.glsl",
                                       "rectangle.fs.glsl");

    /* Create program object for drawing text */
    text_prog = gl_create_prog_from_src("text.vs.glsl", "text.fs.glsl");

    /* Create program object for converting YUYV to RGB */
    yuyv_to_rgb_prog = gl_create_prog_from_src("yuyv-to-rgb.vs.glsl",
                                               "yuyv-to-rgb.fs.glsl");

    /* Create program object for converting RGB to NV12 */
    rgb_to_nv12_prog = gl_create_prog_from_src("rgb-to-nv12.vs.glsl",
                                               "rgb-to-nv12.fs.glsl");

    /* Create resources needed for rendering */
    gl_res = gl_create_resources(FRAME_WIDTH_IN_PIXELS,
                                 FRAME_HEIGHT_IN_PIXELS, FONT_FILE);

    /* Initialize OpenGL ES extension functions */
    assert(gl_init_ext_funcs());

    /**************************************************************************
     *               STEP 3: CREATE TEXTURES FROM YUYV BUFFERS                *
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
    assert(util_is_aligned_to_page_size(YUYV_FRAME_SIZE_IN_BYTES));

    /* Create YUYV EGLImage objects */
    p_yuyv_imgs = egl_create_yuyv_images(display,
                                         FRAME_WIDTH_IN_PIXELS,
                                         FRAME_HEIGHT_IN_PIXELS,
                                         p_data->p_yuyv_bufs,
                                         YUYV_BUFFER_COUNT);
    assert(p_yuyv_imgs != NULL);

    /* Create YUYV textures */
    p_yuyv_texs = gl_create_external_textures(p_yuyv_imgs, YUYV_BUFFER_COUNT);
    assert(p_yuyv_texs != NULL);

    /**************************************************************************
     *          STEP 4: CREATE FRAMEBUFFERS FROM EMPTY RGB TEXTURES           *
     **************************************************************************/

    /* Create RGB textures */
    p_rgb_texs = gl_create_rgb_textures(FRAME_WIDTH_IN_PIXELS,
                                        FRAME_HEIGHT_IN_PIXELS,
                                        NULL, YUYV_BUFFER_COUNT);
    assert(p_rgb_texs != NULL);

    /* Create framebuffers */
    p_rgb_fbs = gl_create_framebuffers(GL_TEXTURE_2D,
                                       p_rgb_texs, YUYV_BUFFER_COUNT);
    assert(p_rgb_fbs != NULL);

    /**************************************************************************
     *               STEP 5: CREATE TEXTURES FROM NV12 BUFFERS                *
     **************************************************************************/

    /* Create NV12 EGLImage objects */
    p_nv12_imgs = egl_create_nv12_images(display,
                                         FRAME_WIDTH_IN_PIXELS,
                                         FRAME_HEIGHT_IN_PIXELS,
                                         p_data->p_nv12_bufs,
                                         NV12_BUFFER_COUNT);
    assert(p_nv12_imgs != NULL);

    /* Create NV12 textures */
    p_nv12_texs = gl_create_external_textures(p_nv12_imgs, NV12_BUFFER_COUNT);
    assert(p_nv12_texs != NULL);

    /**************************************************************************
     *             STEP 6: CREATE FRAMEBUFFERS FROM NV12 TEXTURES             *
     **************************************************************************/

    /* Create framebuffers */
    p_nv12_fbs = gl_create_framebuffers(GL_TEXTURE_EXTERNAL_OES,
                                        p_nv12_texs, NV12_BUFFER_COUNT);
    assert(p_nv12_fbs != NULL);

    /**************************************************************************
     *                       STEP 7: THREAD'S MAIN LOOP                       *
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
         * All subsequent rendering operations will now render to
         * RGB texture which is linked to the framebuffer (see above):
         * https://learnopengl.com/Advanced-OpenGL/Framebuffers */
        glBindFramebuffer(GL_FRAMEBUFFER, p_rgb_fbs[cam_buf.index]);

        /* Convert YUYV texture to RGB texture */
        gl_convert_yuyv(yuyv_to_rgb_prog, GL_TEXTURE_EXTERNAL_OES,
                        p_yuyv_texs[cam_buf.index], gl_res);

        /* Draw rectangle */
        gl_draw_rectangle(rec_prog, gl_res);

        /* Draw text */
        gl_draw_text(text_prog, "This is a text", 25.0f, 25.0f, BLUE, gl_res);

        /* Bind framebuffer.
         * All subsequent rendering operations will now render to
         * NV12 texture which is linked to the framebuffer (see above) */
        glBindFramebuffer(GL_FRAMEBUFFER, p_nv12_fbs[index]);

        /* Convert RGB texture to NV12 texture */
        gl_convert_yuyv(rgb_to_nv12_prog, GL_TEXTURE_2D,
                        p_rgb_texs[cam_buf.index], gl_res);

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
     *                       STEP 8: CLEAN UP OPENGL ES                       *
     **************************************************************************/

    /* Delete framebuffers and NV12 textures */
    gl_delete_framebuffers(p_nv12_fbs, NV12_BUFFER_COUNT);

    gl_delete_textures(p_nv12_texs, NV12_BUFFER_COUNT);
    egl_delete_images(display, p_nv12_imgs, NV12_BUFFER_COUNT);

    /* Delete framebuffers and RGB textyres */
    gl_delete_framebuffers(p_rgb_fbs, YUYV_BUFFER_COUNT);
    gl_delete_textures(p_rgb_texs, YUYV_BUFFER_COUNT);

    /* Delete YUYV textures */
    gl_delete_textures(p_yuyv_texs, YUYV_BUFFER_COUNT);
    egl_delete_images(display, p_yuyv_imgs, YUYV_BUFFER_COUNT);

    /* Delete resources for OpenGL ES */
    gl_delete_resources(gl_res);

    glDeleteProgram(rec_prog);
    glDeleteProgram(text_prog);
    glDeleteProgram(yuyv_to_rgb_prog);
    glDeleteProgram(rgb_to_nv12_prog);

    /**************************************************************************
     *                          STEP 9: CLEAN UP EGL                          *
     **************************************************************************/

    /* Delete EGL context */
    eglDestroyContext(display, context);

    /* Close connection to EGL display */
    egl_disconnect_display(display);

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
