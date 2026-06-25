#version 460 core
#extension GL_ARB_bindless_texture : require

vec2 verts[6] = vec2[](
    vec2(-1, -1), 
    vec2( 1, -1),
    vec2( 1,  1),
    vec2( 1,  1),
    vec2(-1, -1),
    vec2(-1,  1)
);

layout(bindless_image, r16f) uniform image2D flowX_texture;
layout(bindless_image, r16f) uniform image2D flowY_texture;

uniform uvec2 cell_count;
uniform float vector_scale;
uniform uint all_vector_count;

out vec2 uv;

void main() {
    uint vertex_id = gl_VertexID;
    uint vector_id = vertex_id / 6;

    bool is_horizontal = (vector_id < all_vector_count * 0.5);
    uvec2 vector_count = (is_horizontal) ? 
        uvec2(cell_count.x + 1, cell_count.y) : 
        uvec2(cell_count.x, cell_count.y + 1);

    ivec2 vector_coord = ivec2(uvec2(
        vector_id % vector_count.x,
        vector_id / vector_count.y));

    float vector_value = (is_horizontal) ?
        imageLoad(flowX_texture, vector_coord).x :
        imageLoad(flowY_texture, vector_coord).y;

    vec2 vertex_position = verts[vertex_id % 6];
    vec2 vector_position = (2 / vec2(vector_count)) * 2.0 - 1.0;

    vec2 position = vector_position + vertex_position * vector_scale * max(1.0, vector_value);

    uv = vertex_position * 0.5 + 0.5;

    if (!is_horizontal)
         vec2(uv.y, uv.x);

    gl_Position = vec4(position, 0.1, 1.0);
}