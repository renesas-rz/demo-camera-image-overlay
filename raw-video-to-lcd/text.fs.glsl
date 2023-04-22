#version 300 es

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
