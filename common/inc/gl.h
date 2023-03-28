/*******************************************************************************
 * FILENAME: gl.h
 *
 * DESCRIPTION:
 *   OpenGL ES functions.
 *
 * PUBLIC FUNCTIONS:
 *    gl_create_shader
 *    gl_create_prog_from_objs
 *    gl_create_prog_from_src
 *    
 *    gl_is_ext_supported
 *    gl_init_ext_funcs
 *    
 *    gl_create_texture
 *    gl_create_textures
 *    gl_delete_textures
 *    
 *    gl_create_framebuffer
 *    gl_create_framebuffers
 *    gl_delete_framebuffers
 *    
 *    gl_create_resources
 *    gl_delete_resources
 *    
 *    gl_draw_rectangle
 *    gl_conv_yuyv_to_nv12
 *
 * AUTHOR: RVC       START DATE: 15/03/2023
 *
 * CHANGES:
 * 
 ******************************************************************************/

#ifndef _GL_H_
#define _GL_H_

#include <stdint.h>
#include <stdbool.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <GLES3/gl3.h>
#include <GLES3/gl32.h>

#include "egl.h"

/******************************************************************************
 *                           STRUCTURE DEFINITIONS                            *
 ******************************************************************************/

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

/******************************************************************************
 *                            FUNCTION DECLARATION                            *
 ******************************************************************************/

/* Compile the shader from file 'p_file'.
 * Return shader's ID (positive integer) if successful */
GLuint gl_create_shader(const char * p_file, GLenum type);

/* Create program from 2 existing vertex shader and fragment shader objects.
 * Return program's ID (positive integer) if successful */
GLuint gl_create_prog_from_objs(GLuint vs_object, GLuint fs_object);

/* Create program from file 'p_vs_file' and file 'p_fs_file'.
 * Return program's ID (positive integer) if successful */
GLuint gl_create_prog_from_src(const char * p_vs_file, const char * p_fs_file);

/* Return true if extension 'p_name' is supported by the implementation */
bool gl_is_ext_supported(const char * p_name);

/* Initialize OpenGL ES extension functions.
 * Return true if all functions are supported at runtime */
bool gl_init_ext_funcs();

/* Create texture from EGLImage.
 * Return texture's ID (positive integer) if successful.
 *
 * Note: The texture is unbound after calling this function.
 *       Please make sure to bind it when you render it ('glDrawElements')
 *       or adjust its parameters ('glTexParameteri') */
GLuint gl_create_texture(EGLImageKHR image);

/* Create textures from an array of EGLImage objects.
 * Return an array of 'count' textures if successful. Otherwise, return NULL */
GLuint * gl_create_textures(EGLImageKHR * p_images, uint32_t count);

/* Delete textures.
 * Note: This function will deallocate array 'p_textures' */
void gl_delete_textures(GLuint * p_textures, uint32_t count);

/* Create framebuffer from texture.
 * Return framebuffer's ID (positive integer) if successful */
GLuint gl_create_framebuffer(GLuint texture);

/* Create framebuffers from an array of textures.
 * Return an array of 'count' framebuffers if successful. If not, return NULL */
GLuint * gl_create_framebuffers(GLuint * p_textures, uint32_t count);

/* Delete framebuffers.
 * Note: This function will deallocate array 'p_fbs' */
void gl_delete_framebuffers(GLuint * p_fbs, uint32_t count);

/* Create and return resources for OpenGL ES */
gl_resources_t gl_create_resources();

/* Delete resources for OpenGL ES */
void gl_delete_resources(gl_resources_t res);

/* Draw rectangle */
void gl_draw_rectangle(gl_resources_t res);

/* Convert YUYV texture to NV12 texture */
void gl_conv_yuyv_to_nv12(GLuint yuyv_texture, gl_resources_t res);

#endif /* _GL_H_ */
