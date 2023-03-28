#version 300 es
#extension GL_EXT_YUV_target : require

precision mediump float;

in vec3 ourColor;
layout (yuv) out vec4 FragColor;

void main(void)
{
    /* Convert 'ourColor' to YUV format (full range) */
    FragColor = vec4(rgb_2_yuv(ourColor, itu_601_full_range), 1.0);
}
