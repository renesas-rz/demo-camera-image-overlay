#version 300 es

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aYuyvTexCoord;

out vec2 yuyvTexCoord;

void main(void)
{
    /* Just convert a 2D coordinate to 3D coordinate */
    gl_Position = vec4(aPos, 0.0, 1.0);

    /* Pass texture coordinate to fragment shader */
    yuyvTexCoord = aYuyvTexCoord;
}
