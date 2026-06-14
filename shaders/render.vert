#version 460 core

struct Cell {
	vec2 velocity; // m/s
	float density;  // kg/m^2
	float preassure; // Pa
	float temperature; // K
	float padding;
};

layout(std430, binding = 0) buffer cellBuffer {
	Cell cell[];
};

uniform ivec2 simSize;
uniform ivec2 resolution;

out vec4 cellColor;

void main() {
	uint id = gl_VertexID;

	ivec2 coord = ivec2(
        int(id) % simSize.x,
        int(id) / simSize.x
    );

    vec2 uv = (vec2(coord) + 0.5) / vec2(simSize);
    vec2 position = uv * 2.0 - 1.0;

	ivec2 pixelsPerCell = resolution / simSize;

	gl_Position = vec4(position, 0.0, 1.0);
    gl_PointSize = pixelsPerCell.x;

	cellColor = vec4(cell[id].preassure, cell[id].temperature, cell[id].density, 1);
}