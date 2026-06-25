#version 450 core
#extension GL_ARB_bindless_texture : require

layout(bindless_sampler) uniform sampler2D cell_texture;
layout(bindless_sampler) uniform sampler2D flowX_texture;
layout(bindless_sampler) uniform sampler2D flowY_texture;

in vec2 uv;
out vec4 color;

void main() {
    vec4 cell_data = texture(cell_texture, uv);

    color = vec4(cell_data.a, 0.0, -cell_data.a, 1.0);
}