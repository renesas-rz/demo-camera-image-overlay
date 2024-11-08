/* Copyright (c) 2024 Renesas Electronics Corp.
 * SPDX-License-Identifier: MIT-0 */

/*******************************************************************************
 * FILENAME: prog.h
 *
 * DESCRIPTION:
 *   Program functions.
 *
 * PUBLIC FUNCTIONS:
 *   prog_parse_options
 *
 * AUTHOR: RVC       START DATE: 11/11/2024
 *
 ******************************************************************************/

#ifndef _PROG_H_
#define _PROG_H_

#include "util.h"

/******************************************************************************
 *                              MACRO VARIABLES                               *
 ******************************************************************************/

/* Max length of camera device file */
#define CAM_DEV_MAX_LEN 50

/******************************************************************************
 *                            STRUCTURE DEFINITION                            *
 ******************************************************************************/

/* Structure for program options */
typedef struct
{
    /* Camera device file */
    char cam_dev[CAM_DEV_MAX_LEN];

    /* Frame width of camera */
    size_t width;

    /* Frame height of camera */
    size_t height;

    /* Framerate of camera */
    framerate_t framerate;

} prog_opts_t;

/******************************************************************************
 *                            FUNCTION DECLARATION                            *
 ******************************************************************************/

/* Parse program options */
void prog_parse_options(int argc, char * p_argv[], prog_opts_t * p_opts);

#endif /* _PROG_H_ */
