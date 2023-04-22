#include <stdio.h>
#include <assert.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>

#include <sys/time.h>

#include "gl.h"
#include "wl.h"
#include "egl.h"
#include "util.h"
#include "v4l2.h"

/******************************************************************************
 *                              MACRO VARIABLES                               *
 ******************************************************************************/

#define WINDOW_TITLE "Raw video to LCD"

#define FRAME_WIDTH_IN_PIXELS  640

#define FRAME_HEIGHT_IN_PIXELS 480

#define YUYV_FRAME_WIDTH_IN_BYTES (FRAME_WIDTH_IN_PIXELS * 2)

#define YUYV_FRAME_SIZE_IN_BYTES (FRAME_WIDTH_IN_PIXELS  * \
                                  FRAME_HEIGHT_IN_PIXELS * 2)

#define FRAMERATE 30 /* FPS */

/* The sample app is tested OK with:
 *   - Logitech C270 HD Webcam.
 *   - Logitech C920 HD Pro Webcam.
 *   - Logitech C930e Business Webcam.
 *   - Logitech BRIO Ultra HD Pro Business Webcam */
#define USB_CAMERA_FD "/dev/video0"

/* The number of buffers to be allocated for the camera */
#define YUYV_BUFFER_COUNT 5

/******************************************************************************
 *                              GLOBAL VARIABLES                              *
 ******************************************************************************/

/* 1: Interrupt signal */
volatile sig_atomic_t g_int_signal = 0;

/******************************************************************************
 *                           FUNCTION DECLARATIONS                            *
 ******************************************************************************/

void sigint_handler(int signum, siginfo_t * p_info, void * p_ptr);

/******************************************************************************
 *                               MAIN FUNCTION                                *
 ******************************************************************************/

int main()
{
    /* Interrupt signal */
    struct sigaction sig_act;

    /* V4L2 device */
    int cam_fd = -1;

    struct v4l2_format cam_fmt;
    struct v4l2_buffer cam_buf;

    /* YUYV buffers */
    uint32_t index   = 0;
    uint32_t buf_cnt = 0;

    v4l2_dmabuf_exp_t * p_yuyv_bufs = NULL;

    /* YUYV images and textures */
    EGLImageKHR * p_yuyv_imgs = NULL;
    GLuint      * p_yuyv_texs = NULL;

    /* Wayland display and window */
    wl_display_t * p_wl_display = NULL;
    wl_window_t  * p_wl_window  = NULL;

    int ret = 0;

    /* EGL display, config, surface, and context */
    EGLDisplay egl_display = EGL_NO_DISPLAY;
    EGLSurface egl_surface = EGL_NO_SURFACE;
    EGLContext egl_context = EGL_NO_CONTEXT;

    EGLConfig egl_config;

    /* OpenGL ES */
    gl_resources_t gl_resources;

    /* For calculating FPS */
    struct timeval temp_tv;

    int frames = 0;
    int64_t start_us = 0;

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
     *                         STEP 4: SET UP WAYLAND                         *
     **************************************************************************/

    /* Connect to Wayland display */
    p_wl_display = wl_connect_display();
    assert(p_wl_display != NULL);

    /* Create Wayland window */
    p_wl_window = wl_create_window(p_wl_display, WINDOW_TITLE,
                                   FRAME_WIDTH_IN_PIXELS,
                                   FRAME_HEIGHT_IN_PIXELS);
    assert(p_wl_window != NULL);

    /**************************************************************************
     *                           STEP 5: SET UP EGL                           *
     **************************************************************************/

    /* Connect to EGL display */
    egl_display = egl_connect_display((EGLNativeDisplayType)
                                      p_wl_display->p_display, &egl_config);
    assert(egl_display != EGL_NO_DISPLAY);

    /* Create EGL window surface */
    egl_surface = eglCreateWindowSurface(egl_display, egl_config,
                                         (EGLNativeWindowType)
                                         p_wl_window->p_egl_window, NULL);
    assert(egl_surface != EGL_NO_SURFACE);

    /* Create and bind EGL context */
    egl_context = egl_create_context(egl_display, egl_config, egl_surface);
    assert(egl_context != EGL_NO_CONTEXT);

    /* Initialize EGL extension functions */
    assert(egl_init_ext_funcs(egl_display));

    /**************************************************************************
     *                        STEP 6: SET UP OPENGL ES                        *
     **************************************************************************/

    /* Create resources needed for rendering */
    gl_resources = gl_create_resources(FRAME_WIDTH_IN_PIXELS,
                                       FRAME_HEIGHT_IN_PIXELS);

    /* Initialize OpenGL ES extension functions */
    assert(gl_init_ext_funcs());

    /**************************************************************************
     *               STEP 7: CREATE TEXTURES FROM YUYV BUFFERS                *
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
    p_yuyv_imgs = egl_create_yuyv_images(egl_display,
                                         FRAME_WIDTH_IN_PIXELS,
                                         FRAME_HEIGHT_IN_PIXELS,
                                         p_yuyv_bufs,
                                         YUYV_BUFFER_COUNT);
    assert(p_yuyv_imgs != NULL);

    /* Create YUYV textures */
    p_yuyv_texs = gl_create_textures(p_yuyv_imgs, YUYV_BUFFER_COUNT);
    assert(p_yuyv_texs != NULL);

    /**************************************************************************
     *                     STEP 8: PREPARE CAPTURING DATA                     *
     **************************************************************************/

    /* For capturing applications, it is customary to first enqueue all
     * mapped buffers, then to start capturing and enter the read loop.
     *
     * https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/mmap.html */
    assert(v4l2_enqueue_bufs(cam_fd, YUYV_BUFFER_COUNT));

    /* Start capturing */
    assert(v4l2_enable_capturing(cam_fd));

    /**************************************************************************
     *                           STEP 9: MAIN LOOP                            *
     **************************************************************************/

    while (!g_int_signal && !g_window_closed && (ret != -1))
    {
        ret = wl_display_dispatch_pending(p_wl_display->p_display);

        /* Receive camera's buffer */
        assert(v4l2_dequeue_buf(cam_fd, &cam_buf));

        /* Get current time */
        gettimeofday(&temp_tv, NULL);

        /* Set start time if the app just started running */
        if (frames == 0)
        {
            start_us = TIMEVAL_TO_USECS(temp_tv);
        }

        /* Show FPS if the time used to collect frames is more than 5 seconds */
        if ((TIMEVAL_TO_USECS(temp_tv) - start_us) > (5 * USECS_PER_SEC))
        {
            printf("%d frames in 5 seconds: %.1f fps\n", frames, frames / 5.0f);

            frames = 0;
            start_us = TIMEVAL_TO_USECS(temp_tv);
        }

        /* Convert YUYV texture to RGB texture */
        gl_convert_yuyv(p_yuyv_texs[cam_buf.index], gl_resources);

        /* Draw rectangle on RGB texture */
        gl_draw_rectangle(gl_resources);

        /* Draw text on RGB texture */
        gl_draw_text("This is a text", 25.0f, 25.0f, BLUE, gl_resources);

        /* Display to monitor */
        eglSwapBuffers(egl_display, egl_surface);

        /* Collect frame */
        frames++;

        /* Reuse camera's buffer */
        assert(v4l2_enqueue_buf(cam_fd, cam_buf.index));
    }

    /**************************************************************************
     *                      STEP 10: STOP CAPTURING DATA                      *
     **************************************************************************/

    assert(v4l2_disable_capturing(cam_fd));

    /**************************************************************************
     *                      STEP 11: CLEAN UP OPENGL ES                       *
     **************************************************************************/

    /* Delete resources for OpenGL ES */
    gl_delete_resources(gl_resources);

    /**************************************************************************
     *                         STEP 12: CLEAN UP EGL                          *
     **************************************************************************/

    /* Delete EGL context */
    eglDestroyContext(egl_display, egl_context);

    /* Delete EGL window surface */
    eglDestroySurface(egl_display, egl_surface);

    /* Close connection to EGL display */
    egl_disconnect_display(egl_display);

    /**************************************************************************
     *                       STEP 13: CLEAN UP WAYLAND                        *
     **************************************************************************/

    /* Delete Wayland window */
    wl_delete_window(p_wl_window);

    /* Close connection to Wayland display */
    wl_disconnect_display(p_wl_display);

    /**************************************************************************
     *                     STEP 14: CLEAN UP V4L2 DEVICE                      *
     **************************************************************************/

    /* Delete YUYV textures */
    gl_delete_textures(p_yuyv_texs, YUYV_BUFFER_COUNT);
    egl_delete_images(egl_display, p_yuyv_imgs, YUYV_BUFFER_COUNT);

    /* Clean up YUYV buffers */
    v4l2_dealloc_dmabufs(p_yuyv_bufs, YUYV_BUFFER_COUNT);

    /* Close the camera */
    close(cam_fd);

    return 0;
}

/******************************************************************************
 *                            FUNCTION DEFINITIONS                            *
 ******************************************************************************/

void sigint_handler(int signum, siginfo_t * p_info, void * p_ptr)
{
    /* Mark parameters as unused */
    UNUSED(p_ptr);
    UNUSED(p_info);
    UNUSED(signum);

    g_int_signal = 1;
}
