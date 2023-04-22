#version 300 es
#extension GL_EXT_YUV_target : require

precision mediump float;

uniform __samplerExternal2DY2YEXT yuyvTexture;

in vec2 yuyvTexCoord;
layout (yuv) out vec4 FragColor;

void main(void)
{
    FragColor = texture(yuyvTexture, yuyvTexCoord);
}
