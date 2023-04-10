#!/bin/bash

# --------- GLOBAL ---------

UVC_DRIVER="uvcvideo"

CUR_LOG_LEVEL="$( eval "cat /proc/sys/kernel/printk | awk '{ print \$1 }'" )"
MIN_LOG_LEVEL="$( eval "cat /proc/sys/kernel/printk | awk '{ print \$3 }'" )"

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

# ---------- MAIN ----------

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
    ./simple_overlay
fi

# Restore current log level
echo $CUR_LOG_LEVEL > /proc/sys/kernel/printk
