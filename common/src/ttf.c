/*******************************************************************************
 * FILENAME: ttf.c
 *
 * DESCRIPTION:
 *   TrueType font function definition.
 * 
 * NOTE:
 *   For function usage, please refer to 'ttf.h'.
 * 
 * AUTHOR: RVC       START DATE: 19/04/2023
 *
 ******************************************************************************/

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <GLES3/gl3.h>
#include <GLES3/gl32.h>

#include "ttf.h"

/******************************************************************************
 *                            FUNCTION DEFINITION                             *
 ******************************************************************************/

bool ttf_generate(const char * p_file, glyph_t * p_glyphs[GLYPH_ARRAY_LEN])
{
    FT_Library ft;
    FT_Face face;

    unsigned char c = 0;

    /* Check parameters */
    assert((p_file != NULL) && (p_glyphs != NULL));

    /* FreeType functions return a value other than 0 if an error occurred */
    if (FT_Init_FreeType(&ft))
    {
        printf("Error: Failed to init FreeType library\n");
        return false;
    }

    /* Open a TrueType font by its pathname */
    if (FT_New_Face(ft, p_file, 0, &face))
    {
        /* Destroy FreeType library */
        FT_Done_FreeType(ft);

        printf("Error: Failed to load TrueType font '%s'\n", p_file);
        return false;
    }

    /* Set character size */
    if (FT_Set_Char_Size(face, 0, CHAR_SIZE << 6,
                         HORZ_RESOLUTION, VERT_RESOLUTION))
    {
        /* Destroy FreeType library */
        FT_Done_Face(face);
        FT_Done_FreeType(ft);

        printf("Error: Failed to set character size to '%d' pt\n", CHAR_SIZE);
        return false;
    }

    /* Disable byte-alignment restriction */
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    /* Initialize array 'p_glyphs' */
    memset(p_glyphs, 0, sizeof(glyph_t *) * GLYPH_ARRAY_LEN);

    /* Load first 128 characters of ASCII set */
    for (c = 0; c < GLYPH_ARRAY_LEN; c++)
    {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER))
        {
            continue;
        }

        /* Allocate struct 'glyph_t' */
        p_glyphs[c] = malloc(sizeof(glyph_t));

        /* Generate texture */
        p_glyphs[c]->width    = face->glyph->bitmap.width;
        p_glyphs[c]->height   = face->glyph->bitmap.rows;
        p_glyphs[c]->offset_x = face->glyph->bitmap_left;
        p_glyphs[c]->offset_y = face->glyph->bitmap_top;
        p_glyphs[c]->advance  = face->glyph->advance.x >> 6;

        glGenTextures(1, &(p_glyphs[c]->tex_id));
        glBindTexture(GL_TEXTURE_2D, p_glyphs[c]->tex_id);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
                     p_glyphs[c]->width, p_glyphs[c]->height, 0,
                     GL_RED, GL_UNSIGNED_BYTE, face->glyph->bitmap.buffer);

        /* Set texture options */
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    /* Unbind currently bound texture */
    glBindTexture(GL_TEXTURE_2D, 0);

    /* Enable byte-alignment restriction */
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    /* Destroy FreeType library */
    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    return true;
}

void ttf_delete_glyphs(glyph_t * p_glyphs[GLYPH_ARRAY_LEN])
{
    unsigned char c = 0;

    /* Check parameter */
    assert(p_glyphs != NULL);

    for (c = 0; c < GLYPH_ARRAY_LEN; c++)
    {
        if (p_glyphs[c] != NULL)
        {
            glDeleteTextures(1, &(p_glyphs[c]->tex_id));

            free(p_glyphs[c]);
            p_glyphs[c] = NULL;
        }
    }
}
