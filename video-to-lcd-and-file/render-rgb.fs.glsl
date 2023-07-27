#version 300 es

/* Copyright (c) 2023 Renesas Electronics Corp.
 * SPDX-License-Identifier: MIT-0 */

precision mediump float;

in vec2 texCoord;
uniform sampler2D tex;

out vec4 FragColor;

void main()
{
    FragColor = texture(tex, texCoord);
}
