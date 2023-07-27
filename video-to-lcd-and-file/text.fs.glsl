#version 300 es

/* Copyright (c) 2023 Renesas Electronics Corp.
 * SPDX-License-Identifier: MIT-0 */

precision mediump float;

uniform sampler2D text;
uniform vec3 textColor;

in vec2 texCoords;
out vec4 FragColor;

void main(void)
{
    vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, texCoords).r);
    FragColor = vec4(textColor, 1.0) * sampled;
}
