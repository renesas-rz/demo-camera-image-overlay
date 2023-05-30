#version 300 es

/* Copyright (c) 2023 Renesas Electronics Corp.
 * SPDX-License-Identifier: MIT-0 */

precision mediump float;

in vec3 ourColor;
out vec4 FragColor;

void main(void)
{
    FragColor = vec4(ourColor, 1.0);
}
