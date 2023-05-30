/* Copyright (c) 2023 Renesas Electronics Corp.
 * SPDX-License-Identifier: MIT-0 */

/*******************************************************************************
 * FILENAME: util.h
 *
 * DESCRIPTION:
 *   Utility functions.
 *
 * PUBLIC FUNCTIONS:
 *   util_print_errno
 *
 *   util_get_page_aligned_size
 *   util_is_aligned_to_page_size
 *
 *   util_to_uppercase
 *   util_find_whole_str
 *
 *   util_read_file
 *   util_write_file
 *
 * AUTHOR: RVC       START DATE: 14/03/2023
 *
 ******************************************************************************/

#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/******************************************************************************
 *                              FUNCTION MACROS                               *
 ******************************************************************************/

/* Mark variable 'VAR' as unused */
#define UNUSED(VAR) ((void)(VAR))

/* Return smallest integral value not less than 'VAL' and divisible by 'RND'
 * (based on: https://github.com/Xilinx/vcu-omx-il/blob/master/exe_omx/encoder).
 *
 * Examples:
 *   - ROUND_UP(359, 2) -> 360.
 *   - ROUND_UP(480, 2) -> 480.
 *
 *   - ROUND_UP(360, 32)  ->  384.
 *   - ROUND_UP(640, 32)  ->  640.
 *   - ROUND_UP(720, 32)  ->  736.
 *   - ROUND_UP(1280, 32) -> 1280.
 *   - ROUND_UP(1920, 32) -> 1920 */
#define ROUND_UP(VAL, RND) (((VAL) + (RND) - 1) & (~((RND) - 1)))

/* The number of microseconds per milisecond */
#define USECS_PER_MSEC 1000

/* The number of microseconds per second */
#define USECS_PER_SEC 1000000

/* Convert struct 'timeval' to microseconds */
#define TIMEVAL_TO_USECS(T) (((T).tv_sec * USECS_PER_SEC) + (T).tv_usec)

/* Convert struct 'timeval' to miliseconds */
#define TIMEVAL_TO_MSECS(T) ((1.0 * TIMEVAL_TO_USECS(T)) / USECS_PER_MSEC)

/* Convert struct 'timeval' to seconds */
#define TIMEVAL_TO_SECS(T) ((1.0 * TIMEVAL_TO_USECS(T)) / USECS_PER_SEC)

/******************************************************************************
 *                            FUNCTION DECLARATION                            *
 ******************************************************************************/

/* Print error based on 'errno' */
void util_print_errno();

/***************************** FOR PAGE ALIGNMENT *****************************/

/* Return page-aligned value based on 'size' */
size_t util_get_page_aligned_size(size_t size);

/* Check if 'size' is aligned to page size or not?
 * Return true if successful. Otherwise, return false */
bool util_is_aligned_to_page_size(size_t size);

/***************************** FOR STRING SEARCH ******************************/

/* Convert 'p_str' to upper case.
 * Return 'p_str' (useful when passing the function to another function) */
char * util_to_uppercase(char * p_str);

/* Return true if 'p_str' can be found in 'p_str_arr' separated by
 * 'p_delim_str'. Otherwise, return false.
 *
 * For example:
 *   1. str_find_whole_str("Hello World, Friends", ", ", "Friends")
 *      -> Extract words: 'Hello', 'World', and 'Friends' from colons
 *         and spaces. Then, return true because 'Friends' matches.
 *
 *   2. str_find_whole_str("Hello World, Friends", ", ", "Friend")
 *      -> Extract words: 'Hello', 'World', and 'Friends' from colons
 *         and spaces. Then, return false because 'Friend' does not match.
 *
 * Note: The function is not case sensitive */
bool util_find_whole_str(const char * p_str_arr,
                         const char * p_delim_str,
                         const char * p_str);

/****************************** FOR FILE ACCESS *******************************/

/* Read file's contents.
 * Return an array of bytes if successful. Otherwise, return NULL.
 * Note: The content must be freed when no longer used */
char * util_read_file(const char * p_name);

/* Write data to a file.
 * Note: The data is not freed by this function */
void util_write_file(const char * p_name, const char * p_buffer, size_t size);

#endif /* _UTIL_H_ */
