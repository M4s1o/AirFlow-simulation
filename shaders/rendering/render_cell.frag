#version 450 core
#extension GL_ARB_bindless_texture : require

layout(bindless_sampler) uniform sampler2D divergenceData_Texture;
layout(bindless_sampler) uniform sampler2D cell_texture;
layout(bindless_sampler) uniform sampler2D flowX_texture;
layout(bindless_sampler) uniform sampler2D flowY_texture;

uniform int render_mode;
in vec2 uv;

out vec4 color;

void main() {
    vec4 cell_data = texture(cell_texture, uv);
    vec4 divergence_data = texture(divergenceData_Texture, uv);

    switch (render_mode) {
    case 0:
        color = vec4(divergence_data.r, 0.0, -divergence_data.r, 1.0);
        break;
    case 1:
        color = vec4(cell_data.x, 0.0, 0.0, 1.0);
        break;
    default:
        color = vec4(1.0, 1.0, 1.0, 1.0);
   }
}