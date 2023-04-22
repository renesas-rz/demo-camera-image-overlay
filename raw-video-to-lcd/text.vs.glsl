#version 300 es

layout (location = 0) in vec4 aVertex; /* <vec2 pos, vec2 tex> */

out vec2 texCoords;

uniform mat4 projection;

void main(void)
{
    gl_Position = projection * vec4(aVertex.xy, 0.0, 1.0);
    texCoords = aVertex.zw;
}
