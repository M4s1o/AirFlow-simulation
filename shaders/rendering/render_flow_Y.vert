#version 460 core
#extension GL_ARB_bindless_texture : require

vec2 verts[] = vec2[](
    vec2(-0.2, 0  ), 
    vec2(-0.2, 0.5),
    vec2( 0.2, 0.5),

    vec2( 0.2, 0.5),
    vec2(-0.2, 0  ),
    vec2( 0.2, 0  ),

    vec2( 1, 0.5),
    vec2( 0, 1  ),
    vec2(-1, 0.5)
);

layout(bindless_image, r16f) uniform image2D flowY_Texture;

uniform ivec2 cell_count;
uniform vec2 vector_scale;

void main() {
    uint vertex_id = gl_VertexID;
    uint vector_id = vertex_id / 9;

    uvec2 vector_count = uvec2(cell_count.x, cell_count.y + 1);

    ivec2 vector_coord = ivec2(uvec2(
        vector_id % vector_count.x,
        vector_id / vector_count.x));

    float vector_value = imageLoad(flowY_Texture, vector_coord).r;

    vec2 cell_size = 2.0 / vec2(cell_count);

    vec2 vertex_position = verts[vertex_id % 9];
    vec2 vector_position = vec2(vector_coord) * cell_size - 1.0 + vec2(cell_size.x * 0.5, 0.0);

    vec2 vertex_scale = vector_scale * vec2(1.0, min(1.0, vector_value)) * cell_size;
    vec2 position = vector_position + vertex_position * vertex_scale;

    gl_Position = vec4(position, 0.1, 1.0);
}