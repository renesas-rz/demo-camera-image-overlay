/*******************************************************************************
 * FILENAME: ttf.h
 *
 * DESCRIPTION:
 *   TrueType font functions.
 *
 * PUBLIC FUNCTIONS:
 *   ttf_generate
 *   ttf_delete_glyphs
 *
 * AUTHOR: RVC       START DATE: 18/04/2023
 *
 ******************************************************************************/

#ifndef _TTF_H_
#define _TTF_H_

#include <stdbool.h>

#include <ft2build.h>
#include FT_FREETYPE_H

/******************************************************************************
 *                              MACRO VARIABLES                               *
 ******************************************************************************/

#define GLYPH_ARRAY_LEN 128

#define CHAR_SIZE 25 /* pt */

#define HORZ_RESOLUTION 96 /* dpi */
#define VERT_RESOLUTION 96 /* dpi */

/******************************************************************************
 *                           STRUCTURE DEFINITIONS                            *
 ******************************************************************************/

/* https://learnopengl.com/In-Practice/Text-Rendering */
typedef struct
{
    /* Texture's ID (generated from bitmap image of active glyph) */
    unsigned int tex_id;

    /* Width and height (in pixels) of bitmap image */
    int width;
    int height;

    /* Horizontal position (in pixels) of bitmap relative to the origin */
    int offset_x;

    /* Vertical position (in pixels) of bitmap relative to the baseline */
    int offset_y;

    /* Horizontal distance (in pixels) from the origin to the origin of the
     * next glyph */
    unsigned int advance;

} glyph_t;

/******************************************************************************
 *                            FUNCTION DECLARATION                            *
 ******************************************************************************/

/* Generate 'p_glyphs' from TrueType font 'p_file'.
 * Return true and fill 'p_glyphs' if the operation is successful.
 *
 * Notes:
 *   - The elements in 'p_glyphs' is first 128 characters of ASCII table.
 *   - The elements in 'p_glyphs' must be freed when no longer used.
 *   - Some elements in 'p_glyphs' can be NULL even though the return
 *     value is true */
bool ttf_generate(const char * p_file, glyph_t * p_glyphs[GLYPH_ARRAY_LEN]);

/* Free elements in 'p_glyphs' */
void ttf_delete_glyphs(glyph_t * p_glyphs[GLYPH_ARRAY_LEN]);

#endif /* _TTF_H_ */
