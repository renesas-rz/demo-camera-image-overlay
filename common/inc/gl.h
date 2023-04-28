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
 *    gl_create_external_texture
 *    gl_create_external_textures
 *    gl_create_rgb_texture
 *    gl_create_rgb_textures
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
 *    gl_convert_yuyv
 *
 * AUTHOR: RVC       START DATE: 15/03/2023
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

#include "cglm/cglm.h"

#include "egl.h"
#include "ttf.h"

/******************************************************************************
 *                              MACRO VARIABLES                               *
 ******************************************************************************/

#define RECTANGLE_VS "rectangle.vs.glsl"
#define RECTANGLE_FS "rectangle.fs.glsl"

#define YUYV_CONV_VS "yuyv-conv.vs.glsl"
#define YUYV_CONV_FS "yuyv-conv.fs.glsl"

#define TEXT_VS "text.vs.glsl"
#define TEXT_FS "text.fs.glsl"

#define TTF_FILE "LiberationSans-Regular.ttf"

#define BLACK ((color_t){ 0.0f, 0.0f, 0.0f })
#define WHITE ((color_t){ 1.0f, 1.0f, 1.0f })
#define RED   ((color_t){ 1.0f, 0.0f, 0.0f })
#define GREEN ((color_t){ 0.0f, 1.0f, 0.0f })
#define BLUE  ((color_t){ 0.0f, 0.0f, 1.0f })

/******************************************************************************
 *                           STRUCTURE DEFINITIONS                            *
 ******************************************************************************/

typedef float color_t[3]; /* { red, green, blue } */

typedef struct
{
    /* For drawing rectangle */
    GLuint vbo_rec_vertices;
    GLuint ibo_rec_indices;

    /* For converting YUYV */
    GLuint vbo_canvas_vertices;
    GLuint ibo_canvas_indices;

    /* For drawing text */
    GLuint vbo_text_vertices;
    mat4 projection_mat;

    /* An array containing information of glyphs */
    glyph_t * p_glyphs[GLYPH_ARRAY_LEN];

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

/* Create external texture from EGLImage object.
 * Return texture's ID (positive integer) if successful.
 *
 * Note: The texture is unbound after calling this function.
 *       Please make sure to bind it when you render it ('glDrawElements')
 *       or adjust its parameters ('glTexParameteri') */
GLuint gl_create_external_texture(EGLImageKHR image);

/* Create external textures from an array of EGLImage objects.
 * Return an array of 'count' textures if successful. Otherwise, return NULL */
GLuint * gl_create_external_textures(EGLImageKHR * p_images, uint32_t count);

/* Create RGB texture. If 'p_data' is not NULL, fill the texture with it.
 * Return texture's ID (positive integer) if successful */
GLuint gl_create_rgb_texture(uint32_t width, uint32_t height, char * p_data);

/* Create RGB textures (same size, but the content may be empty or different).
 * Return an array of 'count' textures if successful. Otherwise, return NULL */
GLuint * gl_create_rgb_textures(uint32_t width, uint32_t height,
                                char ** pp_data, uint32_t count);

/* Delete textures.
 * Note: This function will deallocate array 'p_textures' */
void gl_delete_textures(GLuint * p_textures, uint32_t count);

/* Create framebuffer from texture.
 * Return framebuffer's ID (positive integer) if successful.
 *
 * Note 1: 'tex_target' should be one of the following values:
 *   - GL_TEXTURE_2D: RGB texture.
 *   - GL_TEXTURE_EXTERNAL_OES: External texture.
 *
 * Note 2: The function is based on:
 *   - https://registry.khronos.org/OpenGL-Refpages/es3.0/
 *   - https://learnopengl.com/Advanced-OpenGL/Framebuffers
 *   - https://www.khronos.org/opengl/wiki/Framebuffer_Object */
GLuint gl_create_framebuffer(GLenum tex_target, GLuint texture);

/* Create framebuffers from an array of textures.
 * Return an array of 'count' framebuffers if successful. If not, return NULL */
GLuint * gl_create_framebuffers(GLuint tex_target,
                                GLuint * p_textures, uint32_t count);

/* Delete framebuffers.
 * Note: This function will deallocate array 'p_fbs' */
void gl_delete_framebuffers(GLuint * p_fbs, uint32_t count);

/* Create and return resources for OpenGL ES */
gl_resources_t gl_create_resources(uint32_t frame_width, uint32_t frame_height);

/* Delete resources for OpenGL ES */
void gl_delete_resources(gl_resources_t res);

/* Draw rectangle.
 *
 * https://learnopengl.com
 * https://en.wikibooks.org/wiki/OpenGL_Programming */
void gl_draw_rectangle(GLuint prog, gl_resources_t res);

/* Convert YUYV texture.
 * The destination format is determined by 'prog' and framebuffer's layout.
 *
 * Note: 'tex_target' should be one of the following values:
 *   - GL_TEXTURE_2D: RGB texture.
 *   - GL_TEXTURE_EXTERNAL_OES: External texture */
void gl_convert_yuyv(GLuint prog, GLenum tex_target,
                     GLuint yuyv_tex, gl_resources_t res);

/* Draw text.
 * https://learnopengl.com/In-Practice/Text-Rendering */
void gl_draw_text(GLuint prog, const char * p_text, float x,
                  float y, color_t color, gl_resources_t res);

#endif /* _GL_H_ */
