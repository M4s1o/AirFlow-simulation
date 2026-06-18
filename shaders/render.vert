#version 460 core
#extension GL_ARB_bindless_texture : require

layout(bindless_image, rgba16f) uniform image2D cellData_Texture;
layout(bindless_image, r16f)    uniform image2D flowX_Texture;
layout(bindless_image, r16f)    uniform image2D flowY_Texture;

uniform uvec2 cell_count;
uniform ivec2 resolution;

out vec4 cellColor;
out vec2 velocity;
out vec2 pointPos;

float interpolateLinear(vec2 position, float left_down, float left_up, float right_up, float right_down);

void main() {
	uint id = gl_VertexID;

	ivec2 cell_coord = ivec2(
        int(id) % cell_count.x,
        int(id) / cell_count.x
    );

    vec2 uv = (vec2(cell_coord) + 0.5) / vec2(cell_count);
    vec2 position = uv * 2.0 - 1.0;

	ivec2 pixelsPerCell = resolution / ivec2(cell_count);

	gl_Position = vec4(position, 0.0, 1.0);
    gl_PointSize = pixelsPerCell.x;

	float velocity_top =    imageLoad(flowY_Texture, ivec2(cell_coord + ivec2(0, 1))).x;
	float velocity_right =  imageLoad(flowX_Texture, ivec2(cell_coord + ivec2(1, 0))).x;
	float velocity_bottom = imageLoad(flowY_Texture, ivec2(cell_coord)).x;
	float velocity_left =   imageLoad(flowX_Texture, ivec2(cell_coord)).x;
	vec2 cell_velocity = vec2((velocity_bottom - velocity_top) / 2, (velocity_left - velocity_right) / 2);

	float divergence = imageLoad(cellData_Texture, ivec2(cell_coord)).a * 0.01;

	cellColor = vec4(
		-divergence,//cell[id].pressure / (101325.0 * 2),
		0.0,//cell[id].temperature / (293.15 * 2),
		divergence,//length(cell_velocity) / 4.0, //cell[id].density / (1.225 * 2),
		1);
	velocity = cell_velocity;
}

float interpolateLinear(vec2 position, float left_down, float left_up, float right_up, float right_down) {
	float up = position.x * right_up + (1.0 - position.x) * left_up;
	float down = position.x * right_down + (1.0 - position.x) * left_down;

	return position.y * up + (1.0 - position.y) * down;
}