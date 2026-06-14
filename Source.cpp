#include "graphics_API.h"
#include "graphics_API_extension.h"

#include <chrono>
#include <random>
#include <ctime>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <gtx/quaternion.hpp>
#include <gtc/type_ptr.hpp>

struct Cell {
	glm::vec2 velocity = { 0.0f, 0.0f }; // m/s
	float density = 0.0f;  // kg/m^2
	float preassure = 0.0f; // Pa
	float temperature = 0.0f; // K
	float padding;
};

struct IndirectCommand {
	GLuint count;
	GLuint instanceCount;
	GLuint first;
	GLuint baseInstance;
};

int main() {
	// ===========================
	// setup
	// ===========================

	// simulation setup
	glm::ivec2 simSize = { 5, 5 };

	// window setup
	glfwInit();
	Window window;
	window.setResizable(false);

	// shaders setup
	std::string shadersDirectory = "shaders/";

	ShaderProgram renderProg;
	renderProg.addShader(GL_VERTEX_SHADER, readFileToString(shadersDirectory + "render.vert"));
	renderProg.addShader(GL_FRAGMENT_SHADER, readFileToString(shadersDirectory + "render.frag"));
	renderProg.compile();

	// multidraw setup
	VAO vao;

	// buffers setup
	bool currentCellBuffer = false;
	Buffer cellBuffer[2] = { 
		Buffer(sizeof(Cell) * simSize.x * simSize.y), 
		Buffer(sizeof(Cell) * simSize.x * simSize.y) 
	};

	for (int x = 0; x < simSize.x; x++) {
		for (int y = 0; y < simSize.y; y++) {
			Cell cell;

			if (x < simSize.x / 2) {
				cell.temperature = 500;
				cell.density = 1;
				cell.preassure = 100;
			}

			cellBuffer[currentCellBuffer].write(&cell, x * simSize.y + y, sizeof(cell));
		}
	}

	// loop setup
	glEnable(GL_PROGRAM_POINT_SIZE);

	while (!window.shouldClose()) {
		window.updateFormat();
		window.clear(GL_COLOR_BUFFER_BIT);
		window.setBackground(0.1, 0.1, 0.1, 1);
		window.setViewportSize(1, 1, 0, 0);
		window.setViewport();

		renderProg.useProgram();

		cellBuffer[currentCellBuffer].bind(GL_SHADER_STORAGE_BUFFER, 0, 0, cellBuffer[currentCellBuffer].getSize());
		cellBuffer[!currentCellBuffer].bind(GL_SHADER_STORAGE_BUFFER, 1, 0, cellBuffer[!currentCellBuffer].getSize());
		currentCellBuffer = !currentCellBuffer;

		glUniform2i(glGetUniformLocation(renderProg.getID(), "simSize"), simSize.x, simSize.y);
		glUniform2i(glGetUniformLocation(renderProg.getID(), "resolution"), window.getFormat()->width, window.getFormat()->height);

		vao.draw(GL_POINTS, simSize.x * simSize.y);

		window.swapBuffers();
		glfwPollEvents();
	}
	return 1;
}