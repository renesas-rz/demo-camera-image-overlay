/* Copyright (c) 2024 Renesas Electronics Corp.
 * SPDX-License-Identifier: MIT-0 */

/*******************************************************************************
 * FILENAME: prog.c
 *
 * DESCRIPTION:
 *   Program function definition.
 * 
 * NOTE:
 *   For function usage, please refer to 'prog.h'.
 * 
 * AUTHOR: RVC       START DATE: 11/11/2024
 *
 ******************************************************************************/

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <getopt.h>

#include "prog.h"

/******************************************************************************
 *                            FUNCTION DEFINITION                             *
 ******************************************************************************/

void prog_parse_options(int argc, char * p_argv[], prog_opts_t * p_opts)
{
    int val = 0;

    int tmp_width  = 0;
    int tmp_height = 0;
    int tmp_num    = 0;
    int tmp_den    = 0;

    const char * p_opt_str = "h:w:d:f:";

    const struct option options[] = 
    {
        { "device", required_argument, NULL, 'd' },
        { "width",  required_argument, NULL, 'w' },
        { "height", required_argument, NULL, 'h' },
        { "fps",    required_argument, NULL, 'f' },
        { NULL,     0,                 NULL,  0  } /* Terminate the array */
    };
 
    /* Check parameters */
    assert((p_argv != NULL) && (p_opts != NULL));
    
    while((val = getopt_long(argc, p_argv, p_opt_str, options, NULL)) != -1)
    {
        switch(val)
        {
            case 'd':
            {
                /* Copy camera device name from args to cam_dev */
                strncpy(p_opts->cam_dev, optarg, CAM_DEV_MAX_LEN - 1);
                p_opts->cam_dev[CAM_DEV_MAX_LEN - 1] = '\0';
            }
            break;

            case 'w':
            {
                tmp_width = atoi(optarg);
                p_opts->width = (tmp_width <= 0) ? 0 : tmp_width;
            }
            break;

            case 'h':
            {
                tmp_height = atoi(optarg);
                p_opts->height = (tmp_height <= 0) ? 0 : tmp_height;
            }
            break;

            case 'f':
            {
                sscanf(optarg, "%d/%d", &tmp_num, &tmp_den);
                p_opts->framerate.num = (tmp_num <= 0) ? 0 : tmp_num;
                p_opts->framerate.den = (tmp_den <= 0) ? 0 : tmp_den;
            }
            break;

            default:
            {
                /* Intentionally left blank */
            }
            break;
        }
    }
}
