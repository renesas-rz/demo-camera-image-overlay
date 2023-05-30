#version 300 es

/* Copyright (c) 2023 Renesas Electronics Corp.
 * SPDX-License-Identifier: MIT-0 */

layout (location = 0) in vec4 aVertex; /* <vec2 pos, vec2 tex> */

out vec2 rgbTexCoord;

void main(void)
{
    /* Just convert a 2D coordinate to 3D coordinate */
    gl_Position = vec4(aVertex.xy, 0.0, 1.0);

    /* Pass texture coordinate to fragment shader */
    rgbTexCoord = aVertex.zw;
}
