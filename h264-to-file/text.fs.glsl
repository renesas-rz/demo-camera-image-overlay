#version 300 es
#extension GL_EXT_YUV_target : require

precision mediump float;

uniform sampler2D text;
uniform vec3 textColor;

in vec2 texCoords;
layout (yuv) out vec4 FragColor;

void main(void)
{
    vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, texCoords).r);
    vec3 textColorYUV = rgb_2_yuv(textColor, itu_601_full_range);

    FragColor = vec4(textColorYUV, 1.0) * sampled;
}
