# Camera Image Overlay Demos

## Table of contents

1. [Target devices](#target-devices)
2. [Supported environments](#supported-environments)
3. [Supported USB cameras](#supported-usb-cameras)
4. [Overview](#overview)
5. [Software](#software)
6. [How to compile demos](#how-to-compile-demos)
7. [How to run demos](#how-to-run-demos)
8. [Revision history](#revision-history)

## Target devices

* [RZ/G2L Evaluation Board Kit](https://www.renesas.com/eu/en/products/microcontrollers-microprocessors/rz-mpus/rzg2l-evkit-rzg2l-evaluation-board-kit).
* [RZ/V2L Evaluation Board Kit](https://www.renesas.com/us/en/products/microcontrollers-microprocessors/rz-mpus/rzv2l-evkit-rzv2l-evaluation-board-kit).

## Supported USB cameras

* [Logitech C270 HD Webcam](https://www.logitech.com/en-us/products/webcams/c270-hd-webcam.960-000694.html).
* [Logitech C930e Business Webcam](https://www.logitech.com/en-us/products/webcams/c930e-business-webcam.960-000971.html).
* [Logitech BRIO Ultra HD Pro Business Webcam](https://www.logitech.com/en-us/products/webcams/brio-4k-hdr-webcam.960-001105.html).
* [Logitech C920 HD Pro Webcam](https://www.logitech.com/en-ch/products/webcams/c920-pro-hd-webcam.960-001055.html) (*).

(*) The first buffer from the camera is likely to be corrupted.  
Note: Other cameras may also work. Please use at your own risks.

## Supported environments

* [VLP 3.0.2](https://github.com/renesas-rz/meta-renesas/tree/BSP-3.0.2).
* [VLP 3.0.2-update1](https://github.com/renesas-rz/meta-renesas/tree/BSP-3.0.2-update1).
* [VLP 3.0.3](https://github.com/renesas-rz/meta-renesas/tree/BSP-3.0.3).

Note: Other environments may also work. Please use at your own risks.

## Software

* **Camera capture:** V4L2, uvcvideo driver (include changes from Renesas to support dmabuf).
* **Text/graphics overlay:** EGL, OpenGL ES (proprietary).
* **H.264 encoding:** OMX IL (proprietary).
* **Display:** Wayland/Weston.

## Overview

The demos explain how to overlay raw video images with text and 2D shape (such as: rectangle). Finally, the images are either encoded to H.264, shown on Wayland/Weston Desktop, or streamed to other device.

| Application | Description |
| ----------- | ----------- |
| h264-to-file | Capture video images from camera -> Overlay the images with text and rectangle -> Encode the images to H.264 data -> Write H.264 data to file. |
| raw-video-to-lcd | Capture video images from camera -> Overlay the images with text and rectangle -> Show the images on Wayland/Weston Desktop. |
| video-to-lcd-and-file | **(To be implemented)** Same as _h264-to-file_ and _raw-video-to-lcd_. The video images are both shown on Wayland/Weston Desktop and encoded to H.264 data which is written to file. |
| h264-to-rtsp | **(To be implemented)** Capture video images from camera -> Overlay the images with text and rectangle -> Encode the images to H.264 data -> Stream with RTSP protocol. |

Note: Camera capture, text/graphics overlay, H.264 encoding, and Wayland/Weston display are hardware accelerated.

### Demo h264-to-file

![Workflow of demo h264-to-file](docs/images/h264-to-file-workflow.png)

![h264-to-file-output.mp4](docs/videos/h264-to-file-output.mp4)

### Demo raw-video-to-lcd

![Workflow of demo raw-video-to-lcd](docs/images/raw-video-to-lcd-workflow.png)

![raw-video-to-lcd-output.mp4](docs/videos/raw-video-to-lcd-output.mp4)

### Source code

| Directory | File name | Summary |
| --------- | --------- | ------- |
| common/inc/cglm | *.h | The directory includes header files for [cglm library](https://github.com/recp/cglm/tree/v0.9.0) which provides utils to help math operations to be fast and quick to write. In our case, we are going to use it to create orthographic projection matrix and translation/scale/rotation matrix. |
| common/inc, common/src | egl.h, egl.c | Contain functions that connect/disconnect EGL display, create EGL context, check EGL extensions, and create/delete EGLImage objects. |
| common/inc, common/src | gl.h, gl.c | Contain struct _gl_res_t_, RGB colors (_BLACK_, _WHITE_...), and functions that create shaders, check OpenGL ES extensions, create/delete YUYV and RGB textures, create/delete framebuffers, create/delete resources, convert YUYV textures, draw rectangle, and render text. |
| common/inc, common/src | mmngr.h, mmngr.c | Contain structs: _mmngr_dmabuf_exp_t_, _mmngr_buf_t_, and functions that allocate/free NV12 buffers. |
| common/inc, common/src | omx.h, omx.c | Contain macros that calculate stride, slice height from video resolution and functions that wait for OMX state, get/set input/output port, allocate/free buffers for input/output ports... |
| common/inc, common/src | queue.h, queue.c | Contain struct _queue_t_ and functions that create/delete queue, check if queue is empty or full, enqueue/dequeue element to/from queue. |
| common/inc, common/src | ttf.h, ttf.c | Contain struct _glyph_t_ and functions that generate/delete an array of _glyph_t_ objects from TrueType font file. |
| common/inc, common/src | v4l2.h, v4l2.c | Contain struct _v4l2_dmabuf_exp_t_ and functions that open/verify device, get/set format and framerate, allocate/free buffers, enqueue/dequeue buffers, enable/disable capturing... |
| common/inc, common/src | wl.h, wl.c | Contain structs: _wl_display_t_, _wl_window_t_, and functions that connect/disconnect Wayland display, create/delete window. |
| common/inc, common/src | util.h, util.c | Contain utility functions. |
| common/ttf | LiberationSans-Regular.ttf | [TrueType font](https://releases.pagure.org/liberation-fonts/liberation-fonts-ttf-2.00.1.tar.gz). |
| h264-to-file | h264-to-file.sh, main.c | Demo _h264-to-file_. The script file will run the demo after reprobing uvcvideo with parameter _allocators=1_. Without it, video frames will contain noises. |
| h264-to-file | yuyv-to-rgb.vs.glsl, yuyv-to-rgb.fs.glsl | Convert YUYV textures to RGB. |
| h264-to-file | rectangle.vs.glsl, rectangle.fs.glsl | Draw rectangle on RGB texture. |
| h264-to-file | text.vs.glsl, text.fs.glsl | Draw text on RGB texture. |
| h264-to-file | rgb-to-nv12.vs.glsl, rgb-to-nv12.fs.glsl | Convert RGB textures to NV12. |
| raw-video-to-lcd | raw-video-to-lcd.sh, main.c | Demo _raw-video-to-lcd_. |
| raw-video-to-lcd | yuyv-to-rgb.vs.glsl, yuyv-to-rgb.fs.glsl | Convert YUYV textures to RGB. |
| raw-video-to-lcd | rectangle.vs.glsl, rectangle.fs.glsl | Draw rectangle on RGB texture. |
| raw-video-to-lcd | text.vs.glsl, text.fs.glsl | Draw text on RGB texture. |

## How to compile demos

* Source the environment setup script of SDK:

  ```bash
  user@ubuntu:~$ source /path/to/sdk/environment-setup-aarch64-poky-linux
  ```

* Go to directory _demo-camera-image-overlay_ and run _make_ command:

  ```bash
  user@ubuntu:~$ cd demo-camera-image-overlay
  user@ubuntu:~/demo-camera-image-overlay$ make
  ```

* After compilation, the demos should be in directories _h264-to-file_ and _raw-video-to-lcd_.

  ```bash
  h264-to-file/
  ├── h264-to-file.sh
  ├── main
  ├── rectangle.fs.glsl
  ├── rectangle.vs.glsl
  ├── rgb-to-nv12.fs.glsl
  ├── rgb-to-nv12.vs.glsl
  ├── text.fs.glsl
  ├── text.vs.glsl
  ├── yuyv-to-rgb.fs.glsl
  ├── yuyv-to-rgb.vs.glsl
  └── LiberationSans-Regular.ttf

  raw-video-to-lcd/
  ├── raw-video-to-lcd.sh
  ├── main
  ├── rectangle.fs.glsl
  ├── rectangle.vs.glsl
  ├── text.fs.glsl
  ├── text.vs.glsl
  ├── yuyv-to-rgb.fs.glsl
  ├── yuyv-to-rgb.vs.glsl
  └── LiberationSans-Regular.ttf
  ```

## How to run demos

### h264-to-file

* After [compilation](#how-to-compile-demos), copy directory _h264-to-file_ to directory _/home/root/_ of RZ/G2L or RZ/V2L board.  
Then, run the following commands:

  ```bash
  root@smarc-rzv2l:~# cd h264-to-file
  root@smarc-rzv2l:~/h264-to-file# chmod 755 h264-to-file.sh
  root@smarc-rzv2l:~/h264-to-file# chmod 755 main
  root@smarc-rzv2l:~/h264-to-file# ./h264-to-file.sh
  ```

* The script _h264-to-file.sh_ should generate the below messages:

  ```bash
  root@smarc-rzv2l:~/h264-to-file# ./h264-to-file.sh
  Removing 'uvcvideo' from kernel
  Adding 'uvcvideo' to kernel
  Running sample application
  V4L2 device:
    Name: 'UVC Camera (046d:0825)'
    Bus: 'usb-11c70100.usb-1.2'
    Driver: 'uvcvideo (v5.10.158)'
  V4L2 format:
    Frame width (pixels): '640'
    Frame height (pixels): '480'
    Bytes per line: '1280'
    Frame size (bytes): '614400'
    Pixel format: 'YUYV'
    Scan type: 'Progressive'
  V4L2 framerate: '30.0'
  OMX media component's role: 'video_encoder.avc'
  OMX state: 'OMX_StateIdle'
  OMX state: 'OMX_StateExecuting'
  EmptyBufferDone exited
  FillBufferDone exited
  FillBufferDone exited
  EmptyBufferDone exited
  FillBufferDone exited
  ...
  ^CEmptyBufferDone exited
  FillBufferDone exited
  OMX event: 'End-of-Stream'
  FillBufferDone exited
  Thread 'thread_input' exited
  Thread 'thread_output' exited
  FillBufferDone exited
  OMX state: 'OMX_StateIdle'
  OMX state: 'OMX_StateLoaded'
  ```

* Press Ctrl-C to exit the demo. The output video will also be generated:

  ```bash
  root@smarc-rzv2l:~/h264-to-file# ls -l out-*
  -rw-r--r-- 1 root root 287961 Sep 20 11:08 out-h264-640x480.264
  ```

* You can open it with [Media Classic Player](https://mpc-hc.org/) on Windows (recommended), [Videos application](https://manpages.ubuntu.com/manpages/trusty/man1/totem.1.html) on Ubuntu, or GStreamer pipeline on [VLP environment](#supported-environments) as below:

  ```bash
  gst-launch-1.0 filesrc location=out-h264-640x480.264 ! h264parse ! omxh264dec ! videorate ! video/x-raw, framerate=30/1 ! waylandsink
  ```

### raw-video-to-lcd

* After [compilation](#how-to-compile-demos), copy directories _raw-video-to-lcd_ to directory _/home/root/_ of RZ/G2L or RZ/V2L board.  
Then, run the following commands:

  ```bash
  root@smarc-rzv2l:~# cd raw-video-to-lcd
  root@smarc-rzv2l:~/raw-video-to-lcd# chmod 755 raw-video-to-lcd.sh
  root@smarc-rzv2l:~/raw-video-to-lcd# chmod 755 main
  root@smarc-rzv2l:~/raw-video-to-lcd# ./raw-video-to-lcd.sh
  ```

* The script _raw-video-to-lcd.sh_ should generate the below messages:

  ```bash
  root@smarc-rzv2l:~/raw-video-to-lcd# ./raw-video-to-lcd.sh
  Removing 'uvcvideo' from kernel
  Adding 'uvcvideo' to kernel
  Running sample application
  V4L2 device:
    Name: 'UVC Camera (046d:0825)'
    Bus: 'usb-11c70100.usb-1.2'
    Driver: 'uvcvideo (v5.10.158)'
  V4L2 format:
    Frame width (pixels): '640'
    Frame height (pixels): '480'
    Bytes per line: '1280'
    Frame size (bytes): '614400'
    Pixel format: 'YUYV'
    Scan type: 'Progressive'
  V4L2 framerate: '30.0'
  149 frames in 5 seconds: 29.8 fps
  149 frames in 5 seconds: 29.8 fps
  ...
  ```

* The demo should be shown on Wayland/Weston desktop. You can press Ctrl-C to exit the demo.

## Revision history

| Version | Date | Summary |
| ------- | ---- | ------- |
| 1.0 | Jan 05, 2023 | Add demo _h264-to-file_. However, it only encodes 1 frame and outputs 1 H.264 frame. It also outputs 3 raw frame for debugging purposes. |
| 1.1 | Feb 23, 2023 | Demo _h264-to-file_ is able to encode multiple frames and outputs 1 H.264 video. |
| 1.2 | Mar 14, 2023 | Convert demo _h264-to-file_ to multi-threaded application for better performance. |
| 2.0 | Apr 06, 2023 | Add demo _raw-video-to-lcd_. |
| 2.1 | Apr 26, 2023 | Render text and transform rectangle. |
