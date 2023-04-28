#version 300 es
#extension GL_EXT_YUV_target : require

precision mediump float;

uniform sampler2D rgbTexture;

in vec2 rgbTexCoord;
layout (yuv) out vec4 FragColor;

void main(void)
{
    /* Re-adjust texture coordinate */
    vec2 flippedTexCoord = vec2(rgbTexCoord.x, 1.0 - rgbTexCoord.y);

    /* Get color of texture */
    vec3 texColor = vec3(texture(rgbTexture, flippedTexCoord));

    /* Convert 'texColor' from RGB format to YUV (full range) */
    FragColor = vec4(rgb_2_yuv(texColor, itu_601_full_range), 1.0);
}
