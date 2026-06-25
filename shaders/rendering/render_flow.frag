#version 450 core
#extension GL_ARB_bindless_texture : require

layout(bindless_sampler) uniform sampler2D arrow_texture;

in vec2 uv;
out vec4 color;

void main() {
    color = texture(arrow_texture, uv);
}