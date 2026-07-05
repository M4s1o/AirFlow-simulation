#version 460 core

vec2 verts[] = vec2[](
    vec2(0  , -0.2), 
    vec2(0.5, -0.2),
    vec2(0.5,  0.2),

    vec2(0.5,  0.2),
    vec2(0  , -0.2),
    vec2(0  ,  0.2),

    vec2(0.5,  1),
    vec2(1  ,  0),
    vec2(0.5, -1)
);

layout(binding = 0) uniform sampler2D flowX_Texture;

uniform ivec2 cell_count;
uniform vec2 vector_scale;

void main() {
    uint vertex_id = gl_VertexID;
    uint vector_id = vertex_id / 9;

    uvec2 vector_count = uvec2(cell_count.x + 1, cell_count.y);

    ivec2 vector_coord = ivec2(uvec2(
        vector_id % vector_count.x,
        vector_id / vector_count.x));

    float vector_value = texelFetch(flowX_Texture, vector_coord, 0).x;

    vec2 cell_size = 2.0 / vec2(cell_count);

    vec2 vertex_position = verts[vertex_id % 9];
    vec2 vector_position = vec2(vector_coord) * cell_size - 1.0 + vec2(0.0, cell_size.y * 0.5);

    vec2 vertex_scale = vec2(vector_scale.y, vector_scale.x) * vec2(min(1.0, vector_value), 1.0) * cell_size;
    vec2 position = vector_position + vertex_position * vertex_scale;

    gl_Position = vec4(position, 0.1, 1.0);
}
