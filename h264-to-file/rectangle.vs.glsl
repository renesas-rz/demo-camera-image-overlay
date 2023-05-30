#version 300 es

/* Copyright (c) 2023 Renesas Electronics Corp.
 * SPDX-License-Identifier: MIT-0 */

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec3 aColor;

uniform mat4 transform;

out vec3 ourColor;

void main(void)
{
    /* Just convert a 2D coordinate to 3D coordinate */
    gl_Position = transform * vec4(aPos, 0.0, 1.0);

    /* Output 'aColor' to the fragment shader */
    ourColor = aColor;
}
