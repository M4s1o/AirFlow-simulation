#version 450 core

uniform vec4 vector_color;

out vec4 color;

void main() {
    color = vec4(vector_color);
}