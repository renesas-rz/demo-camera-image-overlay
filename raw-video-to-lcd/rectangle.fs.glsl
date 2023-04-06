#version 300 es

precision mediump float;

in vec3 ourColor;
out vec4 FragColor;

void main(void)
{
    FragColor = vec4(ourColor, 1.0);
}
