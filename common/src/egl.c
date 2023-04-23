/*******************************************************************************
 * FILENAME: egl.c
 *
 * DESCRIPTION:
 *   EGL function definition.
 * 
 * NOTE:
 *   For function usage, please refer to 'egl.h'.
 * 
 * AUTHOR: RVC       START DATE: 15/03/2023
 *
 ******************************************************************************/

#include <assert.h>
#include <stdlib.h>

#include <drm/drm_fourcc.h>

#include "egl.h"
#include "util.h"

/******************************************************************************
 *                               EGL EXTENSIONS                               *
 ******************************************************************************/

typedef EGLImageKHR (*EGLCREATEIMAGEKHR) (EGLDisplay dpy,
                                          EGLContext ctx,
                                          EGLenum target,
                                          EGLClientBuffer buffer,
                                          EGLint * p_attr_list);

typedef EGLBoolean (*EGLDESTROYIMAGEKHR) (EGLDisplay dpy, EGLImageKHR image);

EGLCREATEIMAGEKHR  eglCreateImageKHR;
EGLDESTROYIMAGEKHR eglDestroyImageKHR;

/******************************************************************************
 *                            FUNCTION DEFINITION                             *
 ******************************************************************************/

EGLDisplay egl_connect_display(NativeDisplayType native, EGLConfig * p_config)
{
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLint count = 0;

    /* Check parameter */
    assert(p_config != NULL);

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

    /* Get default EGL display connection */
    display = eglGetDisplay(native);
    if (display == EGL_NO_DISPLAY)
    {
        printf("Error: Failed to get EGL display\n");
        return EGL_NO_DISPLAY;
    }

    /* Initialize the EGL display connection */
    if (eglInitialize(display, NULL, NULL) == EGL_FALSE)
    {
        printf("Error: Failed to initialize EGL display\n");
        egl_disconnect_display(display);

        return EGL_NO_DISPLAY;
    }

    /* Get a list of EGL frame buffer configurations */
    eglChooseConfig(display, config_attribs, p_config, 1, &count);
    if (count == 0)
    {
        printf("Error: Failed to get EGL frame buffer configurations\n");
        egl_disconnect_display(display);

        return EGL_NO_DISPLAY;
    }

    return display;
}

void egl_disconnect_display(EGLDisplay display)
{
    /* Check parameter */
    assert(display != EGL_NO_DISPLAY);

    /* The currently bound context is marked as no longer current */
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    /* Release resources associated with the EGL display connection */
    eglTerminate(display);
}

EGLContext egl_create_context(EGLDisplay disp, EGLConfig conf, EGLSurface surf)
{
    EGLContext context = EGL_NO_CONTEXT;

    const EGLint ctx_attribs[] =
    {
        /* The requested major version of an OpenGL ES context */
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE,
    };

    /* Check parameter */
    assert(disp != EGL_NO_DISPLAY);

    /* Create an EGL rendering context */
    context = eglCreateContext(disp, conf, EGL_NO_CONTEXT, ctx_attribs);
    if (context == EGL_NO_CONTEXT)
    {
        printf("Error: Failed to create EGL context\n");
        return EGL_NO_CONTEXT;
    }

    /* Attach the context to EGL surfaces */
    if (eglMakeCurrent(disp, surf, surf, context) == EGL_FALSE)
    {
        printf("Error: Failed to bind context\n");
        eglDestroyContext(disp, context);

        return EGL_NO_CONTEXT;
    }

    return context;
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
    if (util_find_whole_str(p_ext_funcs, " ", p_name) == false)
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

EGLImageKHR egl_create_yuyv_image(EGLDisplay display, uint32_t width,
                                  uint32_t height, int dmabuf_fd)
{
    EGLImageKHR img = EGL_NO_IMAGE_KHR;

    /* Check parameters */
    assert(display != EGL_NO_DISPLAY);
    assert((width > 0) && (height > 0) && (dmabuf_fd > 0));

    EGLint img_attribs[] =
    {
        /* The logical dimensions of YUYV buffer in pixels */
        EGL_WIDTH, width,
        EGL_HEIGHT, height,

        /* Pixel format of the buffer, as specified by 'drm_fourcc.h' */
        EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_YUYV,

        /* The dmabuf file descriptor of plane 0 of the image */
        EGL_DMA_BUF_PLANE0_FD_EXT, dmabuf_fd,

        /* The offset from the start of the dmabuf of the first sample in
         * plane 0, in bytes */
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,

        /* The number of bytes between the start of subsequent rows of samples
         * in plane 0 */
        EGL_DMA_BUF_PLANE0_PITCH_EXT, width * 2, /* 2 bytes per pixel */

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
                                     uint32_t width, uint32_t height,
                                     v4l2_dmabuf_exp_t * p_bufs, uint32_t cnt)
{
    EGLImageKHR * p_imgs = NULL;
    uint32_t index = 0;

    /* Check parameters */
    assert(display != EGL_NO_DISPLAY);
    assert((width > 0) && (height > 0));
    assert((p_bufs != NULL) && (cnt > 0));

    p_imgs = (EGLImageKHR *)malloc(cnt * sizeof(EGLImageKHR));

    for (index = 0; index < cnt; index++)
    {
        p_imgs[index] = egl_create_yuyv_image(display, width, height,
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
                                  uint32_t width, uint32_t height,
                                  int y_dmabuf_fd, int uv_dmabuf_fd)
{
    EGLImageKHR img = EGL_NO_IMAGE_KHR;

    /* Check parameters */
    assert(display != EGL_NO_DISPLAY);
    assert((width > 0) && (height > 0));
    assert((y_dmabuf_fd > 0) && (uv_dmabuf_fd > 0));

    EGLint img_attribs[] =
    {
        /* The logical dimensions of NV12 buffer in pixels */
        EGL_WIDTH, width,
        EGL_HEIGHT, height,

        /* Pixel format of the buffer, as specified by 'drm_fourcc.h' */
        EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_NV12,

        EGL_DMA_BUF_PLANE0_FD_EXT, y_dmabuf_fd,
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
        EGL_DMA_BUF_PLANE0_PITCH_EXT, width,

        /* These attribute-value pairs are necessary because NV12 contains 2
         * planes: plane 0 is for Y values and plane 1 is for chroma values */
        EGL_DMA_BUF_PLANE1_FD_EXT, uv_dmabuf_fd,
        EGL_DMA_BUF_PLANE1_OFFSET_EXT, 0,
        EGL_DMA_BUF_PLANE1_PITCH_EXT, width,

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
                                     uint32_t width, uint32_t height,
                                     mmngr_buf_t * p_bufs, uint32_t count)
{
    EGLImageKHR * p_imgs = NULL;

    uint32_t index = 0;
    mmngr_dmabuf_exp_t * p_dmabufs = NULL;

    /* Check parameters */
    assert(display != EGL_NO_DISPLAY);
    assert((width > 0) && (height > 0));
    assert((p_bufs != NULL) && (count > 0));

    p_imgs = (EGLImageKHR *)malloc(count * sizeof(EGLImageKHR));

    for (index = 0; index < count; index++)
    {
        p_dmabufs = p_bufs[index].p_dmabufs;

        p_imgs[index] = egl_create_nv12_image(display, width, height,
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

void egl_delete_images(EGLDisplay display, EGLImageKHR * p_imgs, uint32_t cnt)
{
    uint32_t index = 0;

    /* Check parameters */
    assert((display != EGL_NO_DISPLAY) && (p_imgs != NULL));

    for (index = 0; index < cnt; index++)
    {
        eglDestroyImageKHR(display, p_imgs[index]);
    }

    /* Free entire array */
    free(p_imgs);
}
