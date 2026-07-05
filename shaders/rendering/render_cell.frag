#version 450 core

layout(binding = 0) uniform sampler2D divergence_Texture;
layout(binding = 1) uniform sampler2D pressure_Texture;
layout(binding = 2) uniform sampler2D flowX_Texture;
layout(binding = 3) uniform sampler2D flowY_Texture;

uniform int render_mode;
uniform float render_intensivity;

in vec2 uv;

out vec4 color;

void main() {
    switch (render_mode) {
    case 0:
        float divergence = texture(divergence_Texture, uv).r;
        color = vec4(divergence * render_intensivity, 0.0, -divergence * render_intensivity, 1.0);
        break;
    case 1:
        float pressure = texture(pressure_Texture, uv).r;
        color = vec4(pressure * render_intensivity, 0.0, -pressure * render_intensivity, 1.0);
        break;
    default:
        color = vec4(1.0, 1.0, 1.0, 1.0);
    }
}
