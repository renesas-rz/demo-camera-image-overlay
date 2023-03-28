/*******************************************************************************
 * FILENAME: util.c
 *
 * DESCRIPTION:
 *   Utility function definition.
 * 
 * NOTE:
 *   For function usage, please refer to 'util.h'.
 * 
 * AUTHOR: RVC       START DATE: 14/03/2023
 *
 * CHANGES:
 * 
 ******************************************************************************/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util.h"

/******************************************************************************
 *                            FUNCTION DEFINITION                             *
 ******************************************************************************/

void util_print_errno()
{
    printf("Error: '%s' (code: '%d')\n", strerror(errno), errno);
}

/***************************** FOR PAGE ALIGNMENT *****************************/

size_t util_get_page_aligned_size(size_t size)
{
    /* Get page size */
    const long page_size = sysconf(_SC_PAGESIZE);

    return ROUND_UP(size, page_size);
}

bool util_is_aligned_to_page_size(size_t size)
{
    return (util_get_page_aligned_size(size) == size) ? true : false;
}

/***************************** FOR STRING SEARCH ******************************/

char * util_to_uppercase(char * p_str)
{
    int index = 0;

    /* Check parameter */
    assert(p_str != NULL);

    for (index = 0; p_str[index] != '\0'; index++)
    {
        /* If 'p_str[index]' is neither an 'unsigned char' value or 'EOF',
         * the behavior of function 'toupper' is undefined.
         *
         * Therefore, 'p_str[index]' must be cast to 'unsigned char' */
        p_str[index] = (char)toupper((unsigned char)p_str[index]);
    }

    return p_str;
}

bool util_find_whole_str(const char * p_str_arr,
                         const char * p_delim_str, const char * p_str)
{
    bool b_is_found = false;

    char * p_tmp_str = NULL;
    char * p_tmp_str_arr = NULL;

    char * p_save_str = NULL;
    char * p_token_str = NULL;

    /* Check parameters */
    assert((p_str_arr != NULL) && (p_delim_str != NULL) && (p_str != NULL));

    /* Copy contents of 'p_str' to 'p_tmp_str'.
     * Note: This step is required for function 'str_to_uppercase' */
    p_tmp_str = (char *)malloc(strlen(p_str) + 1);
    strcpy(p_tmp_str, p_str);

    /* Convert 'p_tmp_str' to upper case for string comparison */
    util_to_uppercase(p_tmp_str);

    /* Copy contents of 'p_str_arr' to 'p_tmp_str_arr'.
     *
     * Note: This step is required because 'strtok_r' modifies its first
     * argument. Therefore, variable 'p_str_arr' cannot be used */
    p_tmp_str_arr = (char *)malloc(strlen(p_str_arr) + 1);
    strcpy(p_tmp_str_arr, p_str_arr);

    /* Extract tokens from 'p_tmp_str_arr' */
    p_token_str = strtok_r(p_tmp_str_arr, p_delim_str, &p_save_str);

    while (p_token_str != NULL)
    {
        /* Compare each token with 'p_tmp_str' */
        if (strcmp(util_to_uppercase(p_token_str), p_tmp_str) == 0)
        {
            b_is_found = true;
            break;
        }

        p_token_str = strtok_r(NULL, p_delim_str, &p_save_str);
    }

    /* Free resources */
    free(p_tmp_str);
    free(p_tmp_str_arr);

    return b_is_found;
}

/****************************** FOR FILE ACCESS *******************************/

char * util_read_file(const char * p_name)
{
    long size = -1;
    char * p_content = NULL;

    /* Check parameter */
    assert(p_name != NULL);

    /* Open file */
    FILE * p_fd = fopen(p_name, "r");
    if (p_fd != NULL)
    {
        /* Get file's size */
        fseek(p_fd, 0, SEEK_END);
        size = ftell(p_fd);
        rewind(p_fd);

        /* Prepare buffer to store file's content */
        p_content = (char *)malloc(size + 1);
        memset(p_content, 0, size + 1);

        /* Read file's content and put it into buffer */
        fread(p_content, 1, size, p_fd);
        p_content[size] = '\0';

        /* Close file */
        fclose(p_fd);
    }

    return p_content;
}

void util_write_file(const char * p_name, const char * p_buffer, size_t size)
{
    /* Check parameters */
    assert((p_name != NULL) && (p_buffer != NULL) && (size > 0));

    /* Open file */
    FILE * p_fd = fopen(p_name, "w");
    if (p_fd != NULL)
    {
        /* Write content to the file */
        fwrite(p_buffer, 1, size, p_fd);

        /* Close file */
        fclose(p_fd);
    }
}
