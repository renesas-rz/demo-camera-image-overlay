/* Copyright (c) 2023 Renesas Electronics Corp.
 * SPDX-License-Identifier: MIT-0 */

/*******************************************************************************
 * FILENAME: omx.h
 *
 * DESCRIPTION:
 *   OMX functions.
 *
 * PUBLIC FUNCTIONS:
 *   omx_wait_state
 *
 *   omx_state_to_str
 *   omx_print_mc_role
 *
 *   omx_get_port
 *   omx_get_bitrate_ctrl
 *   omx_set_in_port_fmt
 *   omx_set_out_port_fmt
 *   omx_set_port_buf_cnt
 *
 *   omx_use_buffers
 *   omx_alloc_buffers
 *   omx_dealloc_port_bufs
 *   omx_dealloc_all_port_bufs
 *
 *   omx_get_index
 *   omx_fill_buffers
 *
 * AUTHOR: RVC       START DATE: 14/03/2023
 *
 ******************************************************************************/

#ifndef _OMX_H_
#define _OMX_H_

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include <OMX_Core.h>
#include <OMX_Types.h>
#include <OMX_Component.h>

#include <OMXR_Extension_vecmn.h>
#include <OMXR_Extension_h264e.h>

#include "util.h"
#include "mmngr.h"

/******************************************************************************
 *                              MACRO VARIABLES                               *
 ******************************************************************************/

/* The component name for H.264 encoder media component */
#define RENESAS_VIDEO_ENCODER_NAME "OMX.RENESAS.VIDEO.ENCODER.H264"

/* Introduction to:
 *   OMX_PARAM_PORTDEFINITIONTYPE::format::video::nFrameWidth
 *   (OMX_VIDEO_PORTDEFINITIONTYPE::nFrameWidth)
 *
 * According to OMX IL specification 1.1.2, 'nFrameWidth' is the width of
 * the data in pixels.
 *
 * According to document 'R01USxxxxEJxxxx_vecmn_v1.0.pdf',
 * if 'nFrameWidth' is an odd value, it will be rounded down to the closest
 * even value (*).
 *
 * According to document 'R01USxxxxEJxxxx_h264e_v1.0.pdf',
 * if 'nFrameWidth' is not a multiple of 16, the encoder automatically
 * calculates and adds the left crop information to the encoded stream (**).
 *
 * Warning: Rules (*) and (**) are correct in decoding process, but
 * they are not confirmed in encoding process */


/* Introduction to:
 *   OMX_PARAM_PORTDEFINITIONTYPE::format::video::nFrameHeight
 *   (OMX_VIDEO_PORTDEFINITIONTYPE::nFrameHeight)
 *
 * According to OMX IL specification 1.1.2, 'nFrameHeight' is the height of
 * the data in pixels.
 *
 * According to document 'R01USxxxxEJxxxx_vecmn_v1.0.pdf',
 * if 'nFrameHeight' is an odd value, it will be rounded down to the closest
 * even value (*).
 *
 * According to document 'R01USxxxxEJxxxx_h264e_v1.0.pdf',
 * if 'nFrameHeight' is not a multiple of 16, the encoder automatically
 * calculates and adds the bottom crop information to the encoded stream (**).
 *
 * Warning: Rules (*) and (**) are not confirmed in encoding process */


/* Introduction to:
 *   OMX_PARAM_PORTDEFINITIONTYPE::format::video::xFramerate
 *   (OMX_VIDEO_PORTDEFINITIONTYPE::xFramerate)
 *
 * According to OMX IL specification 1.1.2, 'xFramerate' is the
 * framerate whose unit is frames per second. The value is represented
 * in Q16 format and used on the port which handles uncompressed data.
 *
 * Note: For the list of supported 'xFramerate' values, please refer to
 * 'Table 6-5' in document 'R01USxxxxEJxxxx_h264e_v1.0.pdf' */


/* Introduction to:
 *   OMX_PARAM_PORTDEFINITIONTYPE::nBufferCountActual
 *
 * According to OMX IL specification 1.1.2, 'nBufferCountActual' represents
 * the number of buffers that are required on these ports before they are
 * populated, as indicated by 'OMX_PARAM_PORTDEFINITIONTYPE::bPopulated'.
 *
 * The component shall set a default value no less than 'nBufferCountMin'
 * for this field */


/* Introduction to:
 *   OMX_PARAM_PORTDEFINITIONTYPE::format::video::nStride
 *   (OMX_VIDEO_PORTDEFINITIONTYPE::nStride)
 *
 * Basically, 'nStride' is the sum of 'nFrameWidth' and extra padding pixels
 * at the end of each row of a frame.
 *
 * According to document 'R01USxxxxEJxxxx_vecmn_v1.0.pdf':
 *   - If 'eColorFormat' is 'OMX_COLOR_FormatYUV420SemiPlanar', 'nStride'
 *     must be a multiple of 32 (*).
 *   - If the 'nFrameWidth' exceeds 'nStride', 'OMX_SetParameter' returns
 *     an error.
 *
 * According to document 'R01USxxxxEJxxxx_h264e_v1.0.pdf', the required
 * memory size for buffers of the input port is:
 *   ('nStride' * 'nSlideHeight' * 1.5) * 'nBufferCountActual'
 * instead of:
 *   ('nFrameWidth' * 'nFrameHeight' * 1.5) * 'nBufferCountActual'
 *
 * However, in some cases, 'nStride' is equal to 'nFrameWidth' and
 * 'nSliceHeight' is equal to 'nFrameHeight'.
 *
 * Examples (from section 8.1 in document 'R01USxxxxEJxxxx_h264e_v1.0.pdf'):
 *   1. If 'nFrameWidth' is 720 and
 *      'eColorFormat' is 'OMX_COLOR_FormatYUV420SemiPlanar',
 *      'nStride' will be 736 (even value and is a multiple of 32).
 *   2. If 'nFrameWidth' is 1920 and
 *      'eColorFormat' is 'OMX_COLOR_FormatYUV420SemiPlanar',
 *      'nStride' will be 1920 (even value and is a multiple of 32).
 *
 * Notes:
 *  1. 'nStride' plays an important role in the layout of buffer data
 *     (of the input port) which can be seen in 'Figure 6-6' of document
 *     'R01USxxxxEJxxxx_vecmn_v1.0.pdf'.
 *  2. The sample app uses macro function 'OMX_STRIDE' to calculate 'nStride'
 *     from 'nFrameWidth'.
 *
 * Warning: Rule (*) is not confirmed in encoding process */


/* Introduction to:
 *   OMX_PARAM_PORTDEFINITIONTYPE::format::video::nSliceHeight
 *   (OMX_VIDEO_PORTDEFINITIONTYPE::nSliceHeight)
 *
 * Basically, 'nSliceHeight' is the sum of 'nFrameHeight' and extra
 * padding pixels at the end of each column of a frame.
 *
 * According to document 'R01USxxxxEJxxxx_vecmn_v1.0.pdf':
 *   - If 'nSliceHeight' is an odd value, it will be rounded down to the
 *     closest even value (*).
 *   - If the 'nFrameHeight' exceeds 'nSliceHeight', 'OMX_SetParameter'
 *     returns an error.
 *
 * Examples (from section 8.1 in document 'R01USxxxxEJxxxx_h264e_v1.0.pdf'):
 *   1. If 'nFrameHeight' is 480, 'nSliceHeight' will be 480 (even value).
 *   2. If 'nFrameHeight' is 1080, 'nSliceHeight' will be 1080 (even value).
 *
 * Notes:
 *   1. 'nSliceHeight' plays an important role in the layout of buffer data
 *      (of the input port) which can be seen in 'Figure 6-6' of document
 *      'R01USxxxxEJxxxx_vecmn_v1.0.pdf'.
 *   2. The sample app uses macro function 'OMX_SLICE_HEIGHT' to calculate
 *      'nSliceHeight' from 'nFrameHeight'.
 *
 * Basically, 'nSliceHeight' and 'nStride' are used to calculate start address
 * of plane 1 (UV plane) of NV12 (as below):
 *
 *   'plane1_start_addr' = 'plane0_start_addr' + ('nStride' * 'nSliceHeight')
 *
 *     1. 'plane0_start_addr' is the start address of plane 0 (Y plane) of NV12.
 *        It should be divisible by the page size (4096 bytes).
 *
 *        Note: Function 'int mmngr_alloc_in_user(
 *                            MMNGR_ID *pid,                    (output)
 *                            unsigned long size,               (input)
 *                            unsigned long *pphy_addr,         (output)
 *                            unsigned long *phard_addr,        (output)
 *                            unsigned long *puser_virt_addr,   (output)
 *                            unsigned long flag                (input)
 *                        )'
 *        is one of the suitable choices because pointers 'pphy_addr',
 *        'phard_addr', and 'puser_virt_addr' are divisible by the page size
 *        (see document 'RCH2M2_MMP_MMNGR_Linux_UME_140.pdf').
 *
 *     2. 'plane1_start_addr' is the start address of plane 1 (UV plane) of
 *        NV12. Unlike plane 0, it does not need to be divisible by the page
 *        size in encoding process.
 *
 * Warning: Rule (*) is not confirmed in encoding process */


/* Introduction to:
 *   OMX_PARAM_PORTDEFINITIONTYPE::format::video::eColorFormat
 *   (OMX_VIDEO_PORTDEFINITIONTYPE::eColorFormat)
 *
 * According to OMX IL specification 1.1.2, 'eColorFormat' is the
 * color format of the data of the input port.
 *
 * Note: On G2L, 'eColorFormat' will always be
 * 'OMX_COLOR_FormatYUV420SemiPlanar' which represents NV12 format */


/* Introduction to:
 *   OMX_VIDEO_PARAM_BITRATETYPE::nTargetBitrate
 *
 * According to OMX IL specification 1.1.2, 'nTargetBitrate' is the bitrate in
 * bits per second of the frame to be used on the port which handles compressed
 * data.
 *
 * 'nTargetBitrate' is the same as
 * 'OMX_PARAM_PORTDEFINITIONTYPE::format::video::nBitrate'.
 * When either is updated, the other is updated with the same value
 *
 * Note: Section 6.7.14 in document 'R01USxxxxEJxxxx_h264e_v1.0.pdf'
 * shows valid settings of the bitrate for video encoder */


/* Introduction to:
 *   OMX_PARAM_PORTDEFINITIONTYPE::format::video::eCompressionFormat
 *   (OMX_VIDEO_PORTDEFINITIONTYPE::eCompressionFormat)
 *
 * According to document 'R01USxxxxEJxxxx_h264e_v1.0.pdf',
 * 'eCompressionFormat' only accepts value 'OMX_VIDEO_CodingAVC' */

/******************************************************************************
 *                              FUNCTION MACROS                               *
 ******************************************************************************/

/* The macro is used to populate 'nSize' and 'nVersion' fields of 'P_STRUCT'
 * before passing it to one of the below functions:
 *   - OMX_GetConfig
 *   - OMX_SetConfig
 *   - OMX_GetParameter
 *   - OMX_SetParameter
 */
#define OMX_INIT_STRUCTURE(P_STRUCT)                                \
{                                                                   \
    memset((P_STRUCT), 0, sizeof(*(P_STRUCT)));                     \
                                                                    \
    (P_STRUCT)->nSize = sizeof(*(P_STRUCT));                        \
                                                                    \
    (P_STRUCT)->nVersion.s.nVersionMajor = OMX_VERSION_MAJOR;       \
    (P_STRUCT)->nVersion.s.nVersionMinor = OMX_VERSION_MINOR;       \
    (P_STRUCT)->nVersion.s.nRevision     = OMX_VERSION_REVISION;    \
    (P_STRUCT)->nVersion.s.nStep         = OMX_VERSION_STEP;        \
}

/* Get stride from frame width */
#define OMX_STRIDE(WIDTH) ROUND_UP(WIDTH, 32)

/* Get slice height from frame height */
#define OMX_SLICE_HEIGHT(HEIGHT) ROUND_UP(HEIGHT, 2)

/******************************************************************************
 *                            FUNCTION DECLARATION                            *
 ******************************************************************************/

/* Block calling thread until the component is in state 'state'
 * (based on section 3.2.2.13.2 in OMX IL specification 1.1.2) */
void omx_wait_state(OMX_HANDLETYPE handle, OMX_STATETYPE state);

/* Convert 'OMX_STATETYPE' to string.
 * Return the string (useful when passing the function to 'printf').
 *
 * Note: The string must be freed when no longer used */
char * omx_state_to_str(OMX_STATETYPE state);

/* Print media component's role */
void omx_print_mc_role(OMX_HANDLETYPE handle);

/* Get port's structure 'OMX_PARAM_PORTDEFINITIONTYPE'.
 * Return true if successful. Otherwise, return false.
 *
 * Note: 'port_idx' should be 0 (input port) or 1 (output port) */
bool omx_get_port(OMX_HANDLETYPE handle, OMX_U32 port_idx,
                  OMX_PARAM_PORTDEFINITIONTYPE * p_port);

/* Get video encode bitrate control for port at 'port_idx'.
 * Return true if successful. Otherwise, return false */
bool omx_get_bitrate_ctrl(OMX_HANDLETYPE handle, OMX_U32 port_idx,
                          OMX_VIDEO_PARAM_BITRATETYPE * p_ctrl);

/* Set raw format to input port's structure 'OMX_PARAM_PORTDEFINITIONTYPE'.
 * Return true if successful. Otherwise, return false */
bool omx_set_in_port_fmt(OMX_HANDLETYPE handle,
                         OMX_U32 frame_width, OMX_U32 frame_height,
                         OMX_COLOR_FORMATTYPE color_fmt, OMX_U32 framerate);

/* Set H.264 format and bitrate to output port's structure
 * 'OMX_PARAM_PORTDEFINITIONTYPE'.
 * Return true if successful. Otherwise, return false */
bool omx_set_out_port_fmt(OMX_HANDLETYPE handle, OMX_U32 bitrate,
                          OMX_VIDEO_CODINGTYPE compression_fmt);

/* Set 'buf_cnt' buffers to port 'port_idx'.
 * Return true if successful. Otherwise, return false */
bool omx_set_port_buf_cnt(OMX_HANDLETYPE handle,
                          OMX_U32 port_idx, OMX_U32 buf_cnt);

/* Use buffers in 'p_bufs' to allocate buffer headers for port at 'port_idx'.
 * Return non-NULL value if successful. Otherwise, return NULL.
 *
 * Note: 'p_bufs[index].size' must be equal to 'nBufferSize' of port */
OMX_BUFFERHEADERTYPE ** omx_use_buffers(OMX_HANDLETYPE handle, OMX_U32 port_idx,
                                        mmngr_buf_t * p_bufs, uint32_t count);

/* Allocate buffers and buffer headers for port at 'port_idx'.
 * Return non-NULL value if successful. Otherwise, return NULL */
OMX_BUFFERHEADERTYPE ** omx_alloc_buffers(OMX_HANDLETYPE handle,
                                          OMX_U32 port_idx);

/* Free 'count' elements in 'pp_bufs' */
void omx_dealloc_port_bufs(OMX_HANDLETYPE handle, OMX_U32 port_idx,
                           OMX_BUFFERHEADERTYPE ** pp_bufs, uint32_t count);

/* Free 'nBufferCountActual' elements in 'pp_bufs'.
 * Note: Make sure the length of 'pp_bufs' is equal to 'nBufferCountActual' */
void omx_dealloc_all_port_bufs(OMX_HANDLETYPE handle, OMX_U32 port_idx,
                               OMX_BUFFERHEADERTYPE ** pp_bufs);

/* Get index of element 'p_buf' in array 'pp_bufs'.
 * Return non-negative value if successful */
int omx_get_index(OMX_BUFFERHEADERTYPE * p_buf,
                  OMX_BUFFERHEADERTYPE ** pp_bufs, uint32_t count);

/* Send buffers in 'pp_bufs' to output port.
 * Return true if successful. Otherwise, return false */
bool omx_fill_buffers(OMX_HANDLETYPE handle,
                      OMX_BUFFERHEADERTYPE ** pp_bufs, uint32_t count);

#endif /* _OMX_H_ */
