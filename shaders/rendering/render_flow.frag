#version 450 core

layout(binding = 0) uniform sampler2D arrow_texture;

in vec2 uv;
out vec4 color;

void main() {
    color = texture(arrow_texture, uv);
}
