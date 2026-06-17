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
	float density = 1.0f;  // kg/m^2
	float pressure = 1000.0f; // Pa
	float temperature = 293.15f; // K
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
	glm::ivec2 cellCount = { 50, 50 };
	glm::vec2 simSize = { 2, 2 }; // meters
	float dt = 0.0f;

	// window setup
	glfwInit();
	Window window;
	window.setVsync(false);
	window.setResizable(false);
	window.setSize(0, 0, 800, 800);

	// ImGui setup
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForOpenGL(window.getContext(), true);
	ImGui_ImplOpenGL3_Init("#version 460");
	ImGui::StyleColorsDark();

	// shaders setup
	std::string shadersDirectory = "shaders/";

	ShaderProgram renderProg;
	renderProg.addShader(GL_VERTEX_SHADER, readFileToString(shadersDirectory + "render.vert"));
	renderProg.addShader(GL_FRAGMENT_SHADER, readFileToString(shadersDirectory + "render.frag"));
	renderProg.compile();

	ShaderProgram computeProg;
	computeProg.addShader(GL_COMPUTE_SHADER, readFileToString(shadersDirectory + "air_flow-er.comp"));
	computeProg.compile();

	// draw setup
	VAO vao;

	// buffers setup
	bool currentCellBuffer = false;
	Buffer cellBuffer[2] = { 
		Buffer(sizeof(Cell) * cellCount.x * cellCount.y),
		Buffer(sizeof(Cell) * cellCount.x * cellCount.y)
	};

	for (int x = 0; x < cellCount.x; x++) {
		for (int y = 0; y < cellCount.y; y++) {
			Cell cell;

			// Test 01 - stability test
			//cell.temperature = 293.15f;
			//cell.density = 1.225f;
			//cell.pressure = 101325.0f;

			// Test 02 - partial vacuum
			//if (x < cellCount.x / 2) {
			//	cell.temperature = 293.15f + ((float)rand() / (float)RAND_MAX * 2.0f - 1.0f) * 40.0f;
			//	cell.density = 1.225f + ((float)rand() / (float)RAND_MAX * 2.0f - 1.0f) * 0.2f;
			//	cell.pressure = 101325.0f + ((float)rand() / (float)RAND_MAX * 2.0f - 1.0f) * 1000.0f;
			//}

			// Test 04 - one way
			cell.temperature = 293.15f;
			cell.density = 1.225f;
			cell.pressure = 101325.0f;
			if (x < cellCount.x / 2) {
				cell.velocity = {1, 0};
			}

			cellBuffer[currentCellBuffer].write(&cell, (y * cellCount.x + x) * sizeof(Cell), sizeof(cell));
		}
	}
	// Test 03 - temperature point
	//Cell cell;
	//cell.temperature = 1000.15f;
	//cell.density = 1.225f;
	//cell.pressure = 101325.0f;
	//cellBuffer[currentCellBuffer].write(&cell, (50 * cellCount.y + 50) * sizeof(Cell), sizeof(cell));

	cellBuffer[!currentCellBuffer].write(cellBuffer[currentCellBuffer].getPtr(), 0, cellBuffer[currentCellBuffer].getSize());

	// loop setup
	glEnable(GL_PROGRAM_POINT_SIZE);

	Fence fence;

	while (!window.shouldClose()) {
		window.updateFormat();
		window.clear(GL_COLOR_BUFFER_BIT);
		window.setBackground(0.1, 0.1, 0.1, 1);
		window.setViewportSize(1, 1, 0, 0);
		window.setViewport();

		fence.wait(1000000000);

		currentCellBuffer = !currentCellBuffer;
		cellBuffer[currentCellBuffer].bind(GL_SHADER_STORAGE_BUFFER, 0, 0, cellBuffer[currentCellBuffer].getSize());
		cellBuffer[!currentCellBuffer].bind(GL_SHADER_STORAGE_BUFFER, 1, 0, cellBuffer[!currentCellBuffer].getSize());

		computeProg.useProgram();
		
		glUniform2i(glGetUniformLocation(computeProg.getID(), "cell_count"), cellCount.x, cellCount.y);
		glUniform2f(glGetUniformLocation(computeProg.getID(), "cell_size"), 1.0f / (float)cellCount.x, 1.0f / (float)cellCount.y);
		glUniform1f(glGetUniformLocation(computeProg.getID(), "dt"), dt);
		
		computeProg.runCompute(cellCount.x * cellCount.y, 1, 1, GL_SHADER_STORAGE_BARRIER_BIT);

		renderProg.useProgram();

		glUniform2i(glGetUniformLocation(renderProg.getID(), "cell_count"), cellCount.x, cellCount.y);
		glUniform2i(glGetUniformLocation(renderProg.getID(), "resolution"), window.getFormat()->width, window.getFormat()->height);

		vao.draw(GL_POINTS, cellCount.x * cellCount.y);

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("settings");
		ImGui::SliderFloat("delta time", &dt, 0, 0.0001, "%.7f");
		ImGui::End();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		fence.place();

		window.swapBuffers();
		glfwPollEvents();
	}
	return 1;
}