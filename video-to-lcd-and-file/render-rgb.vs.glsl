#version 300 es

/* Copyright (c) 2023 Renesas Electronics Corp.
 * SPDX-License-Identifier: MIT-0 */

layout(location = 0) in vec4 position;

out vec2 texCoord;

void main()
{
    gl_Position = vec4(position.xy, 0.0, 1.0);
    texCoord = position.zw;
}
