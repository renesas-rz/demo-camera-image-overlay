# Camera Image Overlay Demos

## Table Of Contents

* [General Info](#general-info)
* [Used Technologies](#used-technologies)
* [How To Use](#how-to-use)
* [History](#history)

## General Info

These demo applications show how to use hardware acceleration and transfer data correctly on RZ/G2L and RZ/V2L Series Linux environment.  
Hardware acceleration used in these demo applications include camera (USB), GPU, video encode, display, ...

* h264-to-file: capture from camera, process by GPU, then encode to H264 video and write to file.
* raw-video-to-lcd: capture from camera, process by GPU, then display to the screen.
* (To be implemented) h264-to-rtsp: capture from camera, process by GPU, then encode to H264 video and stream over network by RTSP protocol.
* (To be implemented) video-to-lcd-and-file: capture from camera, process by GPU, then display to the screen and at the same time also encode to H264 file.

GPU processing in these demo applications is overlaying a multi-color rectangle over the raw video image.  
Depends on the application, GPU processing also perform format conversion for video image.

## Used Technologies

* Camera (USB): V4L2, UVCVIDEO (include changes from Renesas to allow dmabuf output)
* GPU processing: OpenGL ES, OpenGL ES SL
* Video processing: H264 OMX encode, GStreamer
* Display: Wayland/Weston, DRM, EGL

Note: OMX encode and GPU processing use Renesas proprietary libraries.  
Other technologies are based on opensource software, but have Renesas' additional changes to improve control over hardware.

## How To Use

* Compile:  
  Source the SDK first (refer to Renesas documents Startup Guide or Release Note).  
  Then compile these demos with make.

  ```bash
  source /opt/poky/3.1.17/environment-setup-aarch64-poky-linux
  cd demo-camera-image-overlay/
  make
  ```

* Execute:  
  Copy whole repository to rootfs of the board.
  Run the sh script in application directory. Stop running by Ctrl-C.

  ```bash
  root@smarc-rzg2l:~/raw-video-to-lcd# ./raw-video-to-lcd.sh
  Removing 'uvcvideo' from kernel
  Adding 'uvcvideo' to kernel
  Running sample application
  V4L2 device:
    Name: 'UVC Camera (046d:0825)'
    Bus: 'usb-11c70100.usb-1.2'
    Driver: 'uvcvideo (v5.10.145)'
  V4L2 format:
    Frame width (pixels): '640'
    Frame height (pixels): '480'
    Bytes per line: '1280'
    Frame size (bytes): '614400'
    Pixel format: 'YUYV'
    Scan type: 'Progressive'
  V4L2 framerate: '30.0'
  ...
  ```

## History

* Version 2.0: Add first version of raw-video-to-lcd application.
* Version 1.2: Improve performance by allowing multi-threads (separate input and ouput threads).
* Version 1.1: Update to allow encode multiple frames (output 1 H264 video file), and improve performance by allowing multiple buffers.
* Version 1.0: First version of h264-to-file application, only encode 1 frame, output 3 raw frames and 1 H264 frame.
