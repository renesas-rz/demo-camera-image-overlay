#!/bin/bash

# Copyright (c) 2023 Renesas Electronics Corp.
# SPDX-License-Identifier: MIT-0

# --------- GLOBAL ---------

UVC_DRIVER="uvcvideo"

CUR_LOG_LEVEL="$( eval "cat /proc/sys/kernel/printk | awk '{ print \$1 }'" )"
MIN_LOG_LEVEL="$( eval "cat /proc/sys/kernel/printk | awk '{ print \$3 }'" )"

# Camera device file
DEFAULT_CAM_DEVICE="/dev/video0"

# Frame width of camera
DEFAULT_WIDTH="640"

# Frame height of camera
DEFAULT_HEIGHT="480"

# Framerate of camera
DEFAULT_FRAMERATE="30/1"

CAM_DEVICE="$DEFAULT_CAM_DEVICE"

WIDTH="$DEFAULT_WIDTH"

HEIGHT="$DEFAULT_HEIGHT"

FRAMERATE="$DEFAULT_FRAMERATE"

# ---------- FUNC ----------

# Check if a kernel module is loaded or not?
#
# $1: Kernel module. For example: 'uvcvideo'...
#
# returns: 0 if loaded
#          1 if not loaded
function is_kernel_module_loaded
{
    RET_VALUE=1

    # Search for the kernel module in file '/proc/modules'
    cat /proc/modules | grep ^$1 > /dev/null 2>&1
    if [ $? -eq 0 ]
    then
        RET_VALUE=0
    fi

    return $RET_VALUE
}

# Function to display usage instructions
function usage
{
    SELF="$( basename "$0" )"
    printf "%b" "Usage: $SELF [OPTIONS]\n"
    printf "%b" "\nOptional arguments:\n"
    printf "%b" "-d, --device\tSpecify the camera's device file (default: $DEFAULT_CAM_DEVICE).\n"
    printf "%b" "-w, --width\tSet the frame width (default: $DEFAULT_WIDTH).\n" 
    printf "%b" "-h, --height\tSet the frame height (default: $DEFAULT_HEIGHT).\n"
    printf "%b" "-f, --fps\tSet the frame rate (default: $DEFAULT_FRAMERATE).\n"
    exit 1
}

# ---------- MAIN ----------

# Parse program options
while [ $# -gt 0 ]
do
    case "$1" in
        -d|--device)
            [ $# -gt 1 ] || usage
            CAM_DEVICE="$2"
            shift 2
            ;;
        -w|--width)
            [ $# -gt 1 ] || usage
            WIDTH="$2"
            shift 2
            ;;
        -h|--height)
            [ $# -gt 1 ] || usage
            HEIGHT="$2"
            shift 2
            ;;
        -f|--fps)
            [ $# -gt 1 ] || usage
            FRAMERATE="$2"
            shift 2
            ;;
        *)
            usage
            ;;
    esac
done

# Ignore SIGINT signal
trap "" SIGINT

# Set current log level to lowest possible value.
# This should ignore most of kernel messages
echo $MIN_LOG_LEVEL > /proc/sys/kernel/printk

# Remove $UVC_DRIVER from kernel if it's loaded
if is_kernel_module_loaded $UVC_DRIVER
then
    echo "Removing '$UVC_DRIVER' from kernel"
    rmmod $UVC_DRIVER
fi

echo "Adding '$UVC_DRIVER' to kernel"

# Add $UVC_DRIVER to kernel with parameter 'allocators' set to 1.
# Without this parameter, output video will contain noises
modprobe $UVC_DRIVER allocators=1
if [ $? -eq 0 ]
then
    echo "Running sample application"
    ./main -d $CAM_DEVICE -w $WIDTH -h $HEIGHT -f $FRAMERATE
fi

# Restore current log level
echo $CUR_LOG_LEVEL > /proc/sys/kernel/printk
