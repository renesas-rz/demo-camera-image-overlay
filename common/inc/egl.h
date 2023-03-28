/*******************************************************************************
 * FILENAME: egl.h
 *
 * DESCRIPTION:
 *   EGL functions.
 *
 * PUBLIC FUNCTIONS:
 *   egl_create_display
 *   egl_delete_display
 *
 *   egl_is_ext_supported
 *   egl_init_ext_funcs
 *
 *   egl_create_yuyv_image
 *   egl_create_yuyv_images
 *   egl_create_nv12_image
 *   egl_create_nv12_images
 *   egl_delete_images
 *
 * AUTHOR: RVC       START DATE: 15/03/2023
 *
 * CHANGES:
 * 
 ******************************************************************************/

#ifndef _EGL_H_
#define _EGL_H_

#include <stdint.h>
#include <stdbool.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglplatform.h>

#include "v4l2.h"
#include "mmngr.h"

/******************************************************************************
 *                            FUNCTION DECLARATION                            *
 ******************************************************************************/

/* Obtain EGL display connection.
 * Return a value other than 'EGL_NO_DISPLAY' if successful */
EGLDisplay egl_create_display();

/* Release current context and terminate EGL display connection */
void egl_delete_display(EGLDisplay display);

/* Return true if 'p_name' is a supported extension to EGL */
bool egl_is_ext_supported(EGLDisplay display, const char * p_name);

/* Initialize EGL extension functions.
 * Return true if all functions are supported at runtime */
bool egl_init_ext_funcs(EGLDisplay display);

/* Create YUYV EGLImage from a dmabuf file descriptor.
 * Return a value other than 'EGL_NO_IMAGE_KHR' if successful */
EGLImageKHR egl_create_yuyv_image(EGLDisplay display, uint32_t img_width,
                                  uint32_t img_height, int dmabuf_fd);

/* Create YUYV EGLImage objects from an array of 'v4l2_dmabuf_exp_t' structs.
 * Return an array of 'cnt' EGLImage objects */
EGLImageKHR * egl_create_yuyv_images(EGLDisplay display,
                                     uint32_t img_width, uint32_t img_height,
                                     v4l2_dmabuf_exp_t * p_bufs, uint32_t cnt);

/* Create NV12 EGLImage from dmabuf file descriptors.
 * Return a value other than 'EGL_NO_IMAGE_KHR' if successful */
EGLImageKHR egl_create_nv12_image(EGLDisplay display,
                                  uint32_t img_width, uint32_t img_height,
                                  int y_dmabuf_fd, int uv_dmabuf_fd);

/* Create NV12 EGLImage objects from an array of 'mmngr_buf_t' structs.
 * Return an array of 'count' EGLImage objects */
EGLImageKHR * egl_create_nv12_images(EGLDisplay display,
                                     uint32_t img_width, uint32_t img_height,
                                     mmngr_buf_t * p_bufs, uint32_t count);

/* Delete an array of EGLImage objects.
 * Note: This function will deallocate array 'p_imgs' */
void egl_delete_images(EGLDisplay display,
                       EGLImageKHR * p_imgs, uint32_t count);

#endif /* _EGL_H_ */
