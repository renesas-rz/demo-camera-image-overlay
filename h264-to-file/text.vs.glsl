#version 300 es

layout (location = 0) in vec4 aVertex; /* <vec2 pos, vec2 tex> */

out vec2 texCoords;

uniform mat4 projection;

void main(void)
{
    vec4 pos = projection * vec4(aVertex.xy, 0.0, 1.0);

    /* For some reasons, the Y-axis is turned upside down.
     * Let's workaround this issue and check it later */
    gl_Position = vec4(pos.x, -1.0 * pos.y, pos.z, pos.w);

    /* Output 'texCoords' to the fragment shader */
    texCoords = aVertex.zw;
}
