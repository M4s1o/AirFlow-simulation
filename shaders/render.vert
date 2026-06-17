#version 460 core

struct Cell {
	vec2 velocity; // m/s
	float density;  // kg/m^2
	float pressure; // Pa
	float temperature; // K
	float padding;
};

layout(std430, binding = 0) buffer cellBuffer {
	Cell cell[];
};

uniform ivec2 cell_count;
uniform ivec2 resolution;

out vec4 cellColor;
out vec2 velocity;
out vec2 pointPos;

void main() {
	uint id = gl_VertexID;

	ivec2 coord = ivec2(
        int(id) % cell_count.x,
        int(id) / cell_count.x
    );

    vec2 uv = (vec2(coord) + 0.5) / vec2(cell_count);
    vec2 position = uv * 2.0 - 1.0;

	ivec2 pixelsPerCell = resolution / cell_count;

	gl_Position = vec4(position, 0.0, 1.0);
    gl_PointSize = pixelsPerCell.x;

	cellColor = vec4(
		0.0,//cell[id].pressure / (101325.0 * 2),
		0.0,//cell[id].temperature / (293.15 * 2),
		length(cell[id].velocity) / 2.0, //cell[id].density / (1.225 * 2),
		1);
	velocity = cell[id].velocity;
}