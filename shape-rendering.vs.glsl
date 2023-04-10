#version 300 es

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec3 aColor;

out vec3 ourColor;

void main(void)
{
    /* For some reasons, the Y-axis is turned upside down.
     * Let's workaround this issue and check it later */
    vec2 newPos = vec2(aPos.x, -1.0 * aPos.y);

    /* Just convert a 2D coordinate to 3D coordinate */
    gl_Position = vec4(newPos, 0.0, 1.0);

    /* Output 'aColor' to the fragment shader */
    ourColor = aColor;
}
