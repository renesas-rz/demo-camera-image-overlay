/* Copyright (c) 2023 Renesas Electronics Corp.
 * SPDX-License-Identifier: MIT-0 */

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

glyph_t ** ttf_generate(const char * p_file)
{
    glyph_t ** pp_glyphs = NULL;

    FT_Library ft;
    FT_Face face;

    unsigned char c = 0;

    /* Check parameter */
    assert(p_file != NULL);

    /* FreeType functions return a value other than 0 if an error occurred */
    if (FT_Init_FreeType(&ft))
    {
        printf("Error: Failed to init FreeType library\n");
        return NULL;
    }

    /* Open a TrueType font by its pathname */
    if (FT_New_Face(ft, p_file, 0, &face))
    {
        /* Destroy FreeType library */
        FT_Done_FreeType(ft);

        printf("Error: Failed to load TrueType font '%s'\n", p_file);
        return NULL;
    }

    /* Set character size */
    if (FT_Set_Char_Size(face, 0, CHAR_SIZE << 6,
                         HORZ_RESOLUTION, VERT_RESOLUTION))
    {
        /* Destroy FreeType library */
        FT_Done_Face(face);
        FT_Done_FreeType(ft);

        printf("Error: Failed to set character size to '%d' pt\n", CHAR_SIZE);
        return NULL;
    }

    /* Disable byte-alignment restriction */
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    /* Generate array 'pp_glyphs' */
    pp_glyphs = calloc(GLYPH_ARRAY_LEN, sizeof(glyph_t *));

    /* Load first 128 characters of ASCII set */
    for (c = 0; c < GLYPH_ARRAY_LEN; c++)
    {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER))
        {
            continue;
        }

        /* Allocate struct 'glyph_t' */
        pp_glyphs[c] = malloc(sizeof(glyph_t));

        /* Generate texture */
        pp_glyphs[c]->width    = face->glyph->bitmap.width;
        pp_glyphs[c]->height   = face->glyph->bitmap.rows;
        pp_glyphs[c]->offset_x = face->glyph->bitmap_left;
        pp_glyphs[c]->offset_y = face->glyph->bitmap_top;
        pp_glyphs[c]->advance  = face->glyph->advance.x >> 6;

        glGenTextures(1, &(pp_glyphs[c]->tex_id));
        glBindTexture(GL_TEXTURE_2D, pp_glyphs[c]->tex_id);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
                     pp_glyphs[c]->width, pp_glyphs[c]->height, 0,
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

    return pp_glyphs;
}

void ttf_delete_glyphs(glyph_t ** pp_glyphs)
{
    unsigned char c = 0;

    /* Check parameter */
    assert(pp_glyphs != NULL);

    for (c = 0; c < GLYPH_ARRAY_LEN; c++)
    {
        if (pp_glyphs[c] != NULL)
        {
            glDeleteTextures(1, &(pp_glyphs[c]->tex_id));
            free(pp_glyphs[c]);
        }
    }

    /* Free entire array */
    free(pp_glyphs);
}
