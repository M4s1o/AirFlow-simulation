#version 450 core

layout(binding = 0) uniform usampler2D obstacle_Texture;

uniform vec4 obstacle_color;

in vec2 uv;

out vec4 color;

void main() {
    uint obstacle = uint(texture(obstacle_Texture, uv).r);
    if (obstacle == 0) {
        color = vec4(0.0, 1.0, 0.0, 0.0);
    }
    else {
        color = obstacle_color;
    }
}