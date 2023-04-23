#version 300 es

layout (location = 0) in vec4 aVertex; /* <vec2 pos, vec2 tex> */

out vec2 yuyvTexCoord;

void main(void)
{
    /* Just convert a 2D coordinate to 3D coordinate */
    gl_Position = vec4(aVertex.xy, 0.0, 1.0);

    /* Pass texture coordinate to fragment shader */
    yuyvTexCoord = aVertex.zw;
}
