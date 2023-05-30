/* Copyright (c) 2023 Renesas Electronics Corp.
 * SPDX-License-Identifier: MIT-0 */

/*******************************************************************************
 * FILENAME: egl.h
 *
 * DESCRIPTION:
 *   EGL functions.
 *
 * PUBLIC FUNCTIONS:
 *   egl_connect_display
 *   egl_disconnect_display
 *
 *   egl_create_context
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

/* Obtain EGL display connection for the native display 'native'.
 * Return a value other than 'EGL_NO_DISPLAY' and update 'p_config' if
 * successful */
EGLDisplay egl_connect_display(NativeDisplayType native, EGLConfig * p_config);

/* Release current context (if any) and close connection to EGL display */
void egl_disconnect_display(EGLDisplay display);

/* Create a new EGL rendering context and then attach it to EGL surfaces.
 * Return a value other than 'EGL_NO_CONTEXT' if successful */
EGLContext egl_create_context(EGLDisplay disp, EGLConfig conf, EGLSurface surf);

/* Return true if 'p_name' is a supported extension to EGL */
bool egl_is_ext_supported(EGLDisplay display, const char * p_name);

/* Initialize EGL extension functions.
 * Return true if all functions are supported at runtime */
bool egl_init_ext_funcs(EGLDisplay display);

/* Create YUYV EGLImage from a dmabuf file descriptor.
 * Return a value other than 'EGL_NO_IMAGE_KHR' if successful */
EGLImageKHR egl_create_yuyv_image(EGLDisplay display, uint32_t width,
                                  uint32_t height, int dmabuf_fd);

/* Create YUYV EGLImage objects from an array of 'v4l2_dmabuf_exp_t' structs.
 * Return an array of 'cnt' EGLImage objects */
EGLImageKHR * egl_create_yuyv_images(EGLDisplay display,
                                     uint32_t width, uint32_t height,
                                     v4l2_dmabuf_exp_t * p_bufs, uint32_t cnt);

/* Create NV12 EGLImage from dmabuf file descriptors.
 * Return a value other than 'EGL_NO_IMAGE_KHR' if successful */
EGLImageKHR egl_create_nv12_image(EGLDisplay display,
                                  uint32_t width, uint32_t height,
                                  int y_dmabuf_fd, int uv_dmabuf_fd);

/* Create NV12 EGLImage objects from an array of 'mmngr_buf_t' structs.
 * Return an array of 'count' EGLImage objects */
EGLImageKHR * egl_create_nv12_images(EGLDisplay display,
                                     uint32_t width, uint32_t height,
                                     mmngr_buf_t * p_bufs, uint32_t count);

/* Delete an array of EGLImage objects.
 * Note: This function will deallocate array 'p_imgs' */
void egl_delete_images(EGLDisplay display, EGLImageKHR * p_imgs, uint32_t cnt);

#endif /* _EGL_H_ */
