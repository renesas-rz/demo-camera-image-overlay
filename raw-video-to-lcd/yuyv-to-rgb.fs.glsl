#version 300 es
#extension GL_EXT_YUV_target : require

precision mediump float;

uniform __samplerExternal2DY2YEXT yuyvTexture;

in vec2 yuyvTexCoord;
out vec4 FragColor;

void main(void)
{
    /* Re-adjust texture coordinate */
    vec2 flippedTexCoord = vec2(yuyvTexCoord.x, 1.0 - yuyvTexCoord.y);

    /* Get color of texture */
    vec3 texColor = vec3(texture(yuyvTexture, flippedTexCoord));

    /* Convert 'texColor' from YUV (full range) to RGB format */
    FragColor = vec4(yuv_2_rgb(texColor, itu_601_full_range), 1.0);
}
