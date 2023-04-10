# demo-camera-image-overlay

## Table of contents
* [General info](#general-info)
* [Used Technologies](#used-technologies)
* [Compile](#compile)
* [History](#history)

## General info
These demo applications show how to use hardware acceleration and transfer data correctly on RZ/G2L and RZ/V2L Series Linux environment.  
Hardware acceleration used in these demo applications include camera (USB), GPU, video encode, display, ...
- h264-to-file: capture from camera, process by GPU, then encode to H264 video and write to file.
- (To be implemented) raw-video-to-lcd: capture from camera, process by GPU, then display to the screen.
- (To be implemented) h264-to-rtsp: capture from camera, process by GPU, then encode to H264 video and stream over network by RTSP protocol
- (To be implemented) video-to-lcd-and-file: capture from camera, process by GPU, then display to the screen and at the same time also encode to H264 file.

GPU processing in these demo applications is overlaying a multi-color rectangle over the raw video image.  
Depends on the application, GPU processing also perform format conversion for video image.

## Used Technologies
* Camera (USB): V4L2, UVCVIDEO (include changes from Renesas to allow dmabuf output)
* GPU processing: OpenGL ES, OpenGL ES SL
* Video processing: H264 OMX encode, GStreamer
* Display: Wayland/Weston, DRM, EGL

Note: OMX encode and GPU processing use Renesas proprietary libraries.  
Other technologies are based on opensource software, but have Renesas' additional changes to improve control over hardware.
	
## Compile
Source the SDK first (refer to Renesas documents Startup Guide or Release Note).  
Then compile these demos with make.
```
$ source /opt/poky/3.1.17/environment-setup-aarch64-poky-linux
$ cd demo-camera-image-overlay/
$ make
```

## History
- Version 1.0: First version of h264-to-file application, only encode 1 frame, output 3 raw frames and 1 H264 frame.
