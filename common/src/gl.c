/*******************************************************************************
 * FILENAME: gl.c
 *
 * DESCRIPTION:
 *   OpenGL ES function definition.
 * 
 * NOTE:
 *   For function usage, please refer to 'gl.h'.
 * 
 * AUTHOR: RVC       START DATE: 15/03/2023
 *
 ******************************************************************************/

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include "gl.h"
#include "util.h"

/******************************************************************************
 *                            OPENGL ES EXTENSIONS                            *
 ******************************************************************************/

typedef void (*GLEGLIMAGETARGETTEXTURE2DOES) (GLenum target,
                                              GLeglImageOES image);

GLEGLIMAGETARGETTEXTURE2DOES glEGLImageTargetTexture2DOES;

/******************************************************************************
 *                            FUNCTION DEFINITION                             *
 ******************************************************************************/

GLuint gl_create_shader(const char * p_file, GLenum type)
{
    GLuint shader = 0;
    char * p_shader_src = NULL;

    GLint log_len = 0;
    char * p_log  = NULL;

    GLint b_compile_ok = GL_FALSE;

    /* Check parameter */
    assert(p_file != NULL);

    p_shader_src = util_read_file(p_file);
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
    if (util_find_whole_str(p_ext_funcs, " ", p_name) == false)
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
        !gl_is_ext_supported("GL_OES_surfaceless_context") ||
        !gl_is_ext_supported("GL_OES_EGL_image_external") ||
        !gl_is_ext_supported("GL_OES_EGL_image_external_essl3") ||
        !gl_is_ext_supported("GL_EXT_YUV_target"))
    {
        printf("Error: Failed to init OpenGL ES extension functions\n");
        return false;
    }

    return true;
}

GLuint gl_create_external_texture(EGLImageKHR image)
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

GLuint * gl_create_external_textures(EGLImageKHR * p_images, uint32_t count)
{
    GLuint * p_textures = NULL;
    uint32_t index = 0;

    /* Check parameters */
    assert((p_images != NULL) && (count > 0));

    p_textures = (GLuint *)malloc(count * sizeof(GLuint));

    for (index = 0; index < count; index++)
    {
        p_textures[index] = gl_create_external_texture(p_images[index]);
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

GLuint gl_create_rgb_texture(uint32_t width, uint32_t height, char * p_data)
{
    GLuint texture = 0;

    /* Check parameters */
    assert((width > 0) && (height > 0));

    /* Create an RGB texture */
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width,
                 height, 0, GL_RGB, GL_UNSIGNED_BYTE, p_data);

    /* 'GL_NEAREST' is the default texture filtering method of OpenGL ES.
     * When set to 'GL_NEAREST', OpenGL ES selects the texel (color and alpha
     * values) which is closest to the texture coordinate */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    /* Clamp the coordinates between 0 and 1. Therefore, higher coordinates
     * become clamped to the edge, resulting in a stretched edge pattern */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    /* Unbind texture */
    glBindTexture(GL_TEXTURE_2D, texture);

    return texture;
}

GLuint * gl_create_rgb_textures(uint32_t width, uint32_t height,
                                char ** pp_data, uint32_t count)
{
    GLuint * p_textures = NULL;

    uint32_t index = 0;
    char * p_data = NULL;

    /* Check parameters */
    assert(count > 0);
    assert((width > 0) && (height > 0));

    p_textures = (GLuint *)malloc(count * sizeof(GLuint));

    for (index = 0; index < count; index++)
    {
        /* Get data */
        p_data = (pp_data == NULL) ? NULL : pp_data[index];

        p_textures[index] = gl_create_rgb_texture(width, height, p_data);
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

GLuint gl_create_framebuffer(GLenum tgt, GLuint tex)
{
    GLuint fb = 0;

    /* Check parameter */
    assert(tex != 0);

    /* Create and bind framebuffer */
    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);

    /* Attach texture to the color buffer of currently bound framebuffer */
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tgt, tex, 0);

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

GLuint * gl_create_framebuffers(GLuint target, GLuint * p_texs, uint32_t count)
{
    GLuint * p_fbs = NULL;
    uint32_t index = 0;

    /* Check parameters */
    assert((p_texs != NULL) && (count > 0));

    p_fbs = (GLuint *)malloc(count * sizeof(GLuint));

    for (index = 0; index < count; index++)
    {
        p_fbs[index] = gl_create_framebuffer(target, p_texs[index]);
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

gl_res_t gl_create_resources(uint32_t width,
                             uint32_t height,
                             const char * p_ttf)
{
    gl_res_t res;

    /* Check parameters */
    assert((p_ttf != NULL) && (width > 0) && (height > 0));

    /* The positions and colors of rectangle */
    GLfloat rec_verts[] =
    {
        /* Rectangle vertices             Colors */
        -0.2f, -0.2f, /* Bottom-left  */  1.0f, 1.0f, 1.0f, /* White */
         0.2f, -0.2f, /* Bottom-right */  1.0f, 0.0f, 0.0f, /* Red   */
         0.2f,  0.2f, /* Top-right    */  0.0f, 1.0f, 0.0f, /* Green */
        -0.2f,  0.2f, /* Top-left     */  0.0f, 0.0f, 1.0f, /* Blue  */
    };

    /* No zooming in/out and convert entire YUYV texture */
    GLfloat cnv_verts[] =
    {
        /* Canvas vertices               Texture vertices */
        -1.0f, -1.0f, /* Bottom-left  */ 0.0f, 0.0f, /* Bottom-left  */
         1.0f, -1.0f, /* Bottom-right */ 1.0f, 0.0f, /* Bottom-right */
         1.0f,  1.0f, /* Top-right    */ 1.0f, 1.0f, /* Top-right    */
        -1.0f,  1.0f, /* Top-left     */ 0.0f, 1.0f, /* Top-left     */
    };

    /* Set of indices that refer to position part of vertex array */
    GLubyte idxs[] =
    {
        0, 1, 2,
        2, 3, 0,
    };

    /* Create projection matrix */
    glm_ortho(0, width, 0, height, 0, 1, res.projection_mat);

    /* Generate glyph array */
    ttf_generate(p_ttf, res.p_glyphs);

    /* Create vertex/index buffer objects and add data to it */
    glGenBuffers(1, &(res.vbo_rec_verts));
    glBindBuffer(GL_ARRAY_BUFFER, res.vbo_rec_verts);
    glBufferData(GL_ARRAY_BUFFER, sizeof(rec_verts), rec_verts, GL_STATIC_DRAW);

    glGenBuffers(1, &(res.vbo_canvas_verts));
    glBindBuffer(GL_ARRAY_BUFFER, res.vbo_canvas_verts);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cnv_verts), cnv_verts, GL_STATIC_DRAW);

    glGenBuffers(1, &(res.vbo_text_verts));
    glBindBuffer(GL_ARRAY_BUFFER, res.vbo_text_verts);
    glBufferData(GL_ARRAY_BUFFER, 4 * 4 * sizeof(float), NULL, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &(res.ibo));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, res.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idxs), idxs, GL_STATIC_DRAW);

    /* Set Viewport */
    glViewport(0, 0, width, height);

    /* Unbind buffers */
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    return res;
}

void gl_delete_resources(gl_res_t res)
{
    /* Delete glyph array */
    ttf_delete_glyphs(res.p_glyphs);

    /* Delete vertex buffer objects */
    glDeleteBuffers(1, &(res.vbo_rec_verts));
    glDeleteBuffers(1, &(res.vbo_canvas_verts));
    glDeleteBuffers(1, &(res.vbo_text_verts));

    /* Delete index buffer object */
    glDeleteBuffers(1, &(res.ibo));
}

void gl_draw_rectangle(GLuint prog, gl_res_t res)
{
    GLint tmp_cnt = 0;

    /* Check parameter */
    assert(prog != 0);

    /* Use program object for drawing rectangle */
    glUseProgram(prog);

    /* Enable attribute 0 since it's disabled by default */
    glEnableVertexAttribArray(0);

    /* Enable attribute 1 since it's disabled by default */
    glEnableVertexAttribArray(1);

    /* Show OpenGL ES how the vertex array should be interpreted */
    glBindBuffer(GL_ARRAY_BUFFER, res.vbo_rec_verts);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                          5 * sizeof(GLfloat), (void *)0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                          5 * sizeof(GLfloat),
                          (void *)(2 * sizeof(GLfloat)));

    /* Draw rectangle */
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, res.ibo);
    glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &tmp_cnt);

    glDrawElements(GL_TRIANGLES, tmp_cnt, GL_UNSIGNED_BYTE, 0);

    /* Wait until 'glDrawElements' finishes */
    glFinish();

    /* Unbind buffers */
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    /* Disable attributes */
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
}

void gl_convert_yuyv(GLuint prog, GLenum target, GLuint yuyv_tex, gl_res_t res)
{
    GLint tmp_cnt = 0;

    /* Check parameter */
    assert((prog != 0) && (yuyv_tex != 0));

    /* Use program object for converting YUYV */
    glUseProgram(prog);

    /* Enable attribute 0 since it's disabled by default */
    glEnableVertexAttribArray(0);

    /* Show OpenGL ES how the vertex array should be interpreted */
    glBindBuffer(GL_ARRAY_BUFFER, res.vbo_canvas_verts);

    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE,
                          4 * sizeof(GLfloat), (void *)0);

    /* Bind the YUYV texture */
    glBindTexture(target, yuyv_tex);

    /* Convert YUYV texture */
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, res.ibo);
    glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &tmp_cnt);

    glDrawElements(GL_TRIANGLES, tmp_cnt, GL_UNSIGNED_BYTE, 0);

    /* Wait until 'glDrawElements' finishes */
    glFinish();

    /* Unbind texture */
    glBindTexture(target, 0);

    /* Unbind buffers */
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    /* Disable attribute */
    glDisableVertexAttribArray(0);
}

void gl_draw_text(GLuint prog, const char * p_text,
                  float x, float y, color_t color, gl_res_t res)
{
    uint32_t index = 0;

    float pos_x = 0.0f;
    float pos_y = 0.0f;

    int width  = 0;
    int height = 0;

    GLint tmp_cnt = 0;

    GLint u_text_color = 0;
    GLint u_projection = 0;

    glyph_t * p_glyph = NULL;

    /* Check parameter */
    assert((prog != 0) && (p_text != NULL));

    /* Enable blending */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* Use program object for drawing text */
    glUseProgram(prog);

    /* Set text color to uniform variable */
    u_text_color = glGetUniformLocation(prog, "textColor");
    glUniform3f(u_text_color, color[0], color[1], color[2]);

    /* Set projection matrix to uniform variable */
    u_projection = glGetUniformLocation(prog, "projection");
    glUniformMatrix4fv(u_projection, 1, GL_FALSE, res.projection_mat[0]);

    /* Enable attribute 0 since it's disabled by default */
    glEnableVertexAttribArray(0);

    /* Show OpenGL ES how the vertex array should be interpreted */
    glBindBuffer(GL_ARRAY_BUFFER, res.vbo_text_verts);

    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE,
                          4 * sizeof(GLfloat), (void *)0);

    /* Iterate through all characters */
    for (index = 0; index < strlen(p_text); index++)
    {
        p_glyph = res.p_glyphs[(unsigned char)p_text[index]];

        if (p_glyph != NULL)
        {
            width  = p_glyph->width;
            height = p_glyph->height;

            pos_x = x + p_glyph->offset_x;
            pos_y = y - (height - p_glyph->offset_y);

            float text_verts[4][4] =
            {
                { pos_x        , pos_y + height, 0.0f, 0.0f }, /*Bottom-left */
                { pos_x + width, pos_y + height, 1.0f, 0.0f }, /*Bottom-right*/
                { pos_x + width, pos_y         , 1.0f, 1.0f }, /*Top-right   */
                { pos_x        , pos_y         , 0.0f, 1.0f }  /*Top-left    */
            };

            /* Update VBO */
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(text_verts), text_verts);

            /* Bind bitmap texture */
            glBindTexture(GL_TEXTURE_2D, p_glyph->tex_id);

            /* Draw texture */
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, res.ibo);
            glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER,
                                   GL_BUFFER_SIZE, &tmp_cnt);

            glDrawElements(GL_TRIANGLES, tmp_cnt, GL_UNSIGNED_BYTE, 0);

            /* Prepare for the next character */
            x += p_glyph->advance;
        }
    }

    /* Wait until 'glDrawElements' finishes */
    glFinish();

    /* Unbind texture */
    glBindTexture(GL_TEXTURE_2D, 0);

    /* Unbind VBO buffer */
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    /* Disable attribute */
    glDisableVertexAttribArray(0);

    /* Disable blending */
    glDisable(GL_BLEND);
}
