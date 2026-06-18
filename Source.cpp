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

void randomizeSimulation(
	Texture cellData[2],
	Texture flowX[2],
	Texture flowY[2],
	glm::uvec2 cellCount);

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
	//window.setVsync(false);
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

	ShaderProgram pressureComputeProg;
	pressureComputeProg.addShader(GL_COMPUTE_SHADER, readFileToString(shadersDirectory + "pressure_solver.comp"));
	pressureComputeProg.compile();

	ShaderProgram velocityXComputeProg;
	velocityXComputeProg.addShader(GL_COMPUTE_SHADER, readFileToString(shadersDirectory + "velocity_X_solver.comp"));
	velocityXComputeProg.compile();

	ShaderProgram velocityYComputeProg;
	velocityYComputeProg.addShader(GL_COMPUTE_SHADER, readFileToString(shadersDirectory + "velocity_Y_solver.comp"));
	velocityYComputeProg.compile();

	// draw setup
	VAO vao;

	// data setup
	Texture cellData[2] = {
		Texture(GL_TEXTURE_2D, 1, GL_RGBA16F, cellCount.x, cellCount.y),
		Texture(GL_TEXTURE_2D, 1, GL_RGBA16F, cellCount.x, cellCount.y)
	};
	Texture flowX[2] = {
		Texture(GL_TEXTURE_2D, 1, GL_R16F, cellCount.x + 1, cellCount.y),
		Texture(GL_TEXTURE_2D, 1, GL_R16F, cellCount.x + 1, cellCount.y)
	};
	Texture flowY[2] = {
		Texture(GL_TEXTURE_2D, 1, GL_R16F, cellCount.x, cellCount.y + 1),
		Texture(GL_TEXTURE_2D, 1, GL_R16F, cellCount.x, cellCount.y + 1)
	};
	bool current_data = false;
	bool current_flow_data = false;

	cellData[current_data].loadImage(GL_READ_WRITE, 0, GL_FALSE, 0, GL_RGBA16F);
	flowX[current_flow_data].loadImage(GL_READ_WRITE, 0, GL_FALSE, 0, GL_R16F);
	flowY[current_flow_data].loadImage(GL_READ_WRITE, 0, GL_FALSE, 0, GL_R16F);

	cellData[!current_data].loadImage(GL_READ_WRITE, 0, GL_FALSE, 0, GL_RGBA16F);
	flowX[!current_flow_data].loadImage(GL_READ_WRITE, 0, GL_FALSE, 0, GL_R16F);
	flowY[!current_flow_data].loadImage(GL_READ_WRITE, 0, GL_FALSE, 0, GL_R16F);

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

		// velocity solve
		velocityXComputeProg.useProgram();
		
		glUniformHandleui64ARB(glGetUniformLocation(velocityXComputeProg.getID(), "cellData_Texture_in"), cellData[current_data].getImageHandle());
		glUniformHandleui64ARB(glGetUniformLocation(velocityXComputeProg.getID(), "flowX_Texture_in"), flowX[current_flow_data].getImageHandle());

		glUniformHandleui64ARB(glGetUniformLocation(velocityXComputeProg.getID(), "cellData_Texture_out"), cellData[!current_data].getImageHandle());
		glUniformHandleui64ARB(glGetUniformLocation(velocityXComputeProg.getID(), "flowX_Texture_out"), flowX[!current_flow_data].getImageHandle());

		glUniform2ui(glGetUniformLocation(velocityXComputeProg.getID(), "cell_count"), cellCount.x, cellCount.y);
		glUniform1f(glGetUniformLocation(velocityXComputeProg.getID(), "K"), dt / (1.225 * ((1.0f / (float)cellCount.x) * (1.0f / (float)cellCount.y))));

		velocityXComputeProg.runCompute(cellCount.x + 1, cellCount.y, 1, GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);


		velocityYComputeProg.useProgram();

		glUniformHandleui64ARB(glGetUniformLocation(velocityYComputeProg.getID(), "cellData_Texture_in"), cellData[current_data].getImageHandle());
		glUniformHandleui64ARB(glGetUniformLocation(velocityYComputeProg.getID(), "flowY_Texture_in"), flowY[current_flow_data].getImageHandle());

		glUniformHandleui64ARB(glGetUniformLocation(velocityYComputeProg.getID(), "cellData_Texture_out"), cellData[!current_data].getImageHandle());
		glUniformHandleui64ARB(glGetUniformLocation(velocityYComputeProg.getID(), "flowY_Texture_out"), flowY[!current_flow_data].getImageHandle());

		glUniform2ui(glGetUniformLocation(velocityYComputeProg.getID(), "cell_count"), cellCount.x, cellCount.y);
		glUniform1f(glGetUniformLocation(velocityYComputeProg.getID(), "K"), dt / (1.225 * ((1.0f / (float)cellCount.x) * (1.0f / (float)cellCount.y))));

		velocityYComputeProg.runCompute(cellCount.x, cellCount.y + 1, 1, GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		current_flow_data = !current_flow_data;

		// pressure solve
		//for (int i = 0; i < 30; i++) {
			pressureComputeProg.useProgram();

			glUniformHandleui64ARB(glGetUniformLocation(pressureComputeProg.getID(), "cellData_Texture_in"), cellData[current_data].getImageHandle());
			glUniformHandleui64ARB(glGetUniformLocation(pressureComputeProg.getID(), "flowX_Texture_in"), flowX[current_flow_data].getImageHandle());
			glUniformHandleui64ARB(glGetUniformLocation(pressureComputeProg.getID(), "flowY_Texture_in"), flowY[current_flow_data].getImageHandle());

			glUniformHandleui64ARB(glGetUniformLocation(pressureComputeProg.getID(), "cellData_Texture_out"), cellData[!current_data].getImageHandle());
			glUniformHandleui64ARB(glGetUniformLocation(pressureComputeProg.getID(), "flowX_Texture_out"), flowX[!current_flow_data].getImageHandle());
			glUniformHandleui64ARB(glGetUniformLocation(pressureComputeProg.getID(), "flowY_Texture_out"), flowY[!current_flow_data].getImageHandle());

			glUniform2ui(glGetUniformLocation(pressureComputeProg.getID(), "cell_count"), cellCount.x, cellCount.y);
			glUniform2f(glGetUniformLocation(pressureComputeProg.getID(), "cell_size"), 1.0f / (float)cellCount.x, 1.0f / (float)cellCount.y);
			glUniform1f(glGetUniformLocation(pressureComputeProg.getID(), "K"), dt / (1.225 * ((1.0f / (float)cellCount.x) * (1.0f / (float)cellCount.y))));
			glUniform1f(glGetUniformLocation(pressureComputeProg.getID(), "SOR"), 1.0f);

			pressureComputeProg.runCompute(cellCount.x, cellCount.y, 1, GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			current_data = !current_data;
		//}

		// render
		renderProg.useProgram();

		glUniformHandleui64ARB(glGetUniformLocation(renderProg.getID(), "cellData_Texture"), cellData[current_data].getImageHandle());
		glUniformHandleui64ARB(glGetUniformLocation(renderProg.getID(), "flowX_Texture"), flowX[current_data].getImageHandle());
		glUniformHandleui64ARB(glGetUniformLocation(renderProg.getID(), "flowY_Texture"), flowY[current_data].getImageHandle());

		glUniform2ui(glGetUniformLocation(renderProg.getID(), "cell_count"), cellCount.x, cellCount.y);
		glUniform2i(glGetUniformLocation(renderProg.getID(), "resolution"), window.getFormat()->width, window.getFormat()->height);

		vao.draw(GL_POINTS, cellCount.x * cellCount.y);

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("settings");
		ImGui::SliderFloat("delta time", &dt, 0, 1, "%.7f");
		if (ImGui::Button("randomize")) {
			randomizeSimulation(cellData, flowX, flowY, cellCount);
		}
		ImGui::End();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		fence.place();

		window.swapBuffers();
		glfwPollEvents();
	}
	return 1;
}

void randomizeSimulation(
	Texture cellData[2],
	Texture flowX[2],
	Texture flowY[2],
	glm::uvec2 cellCount)
{
	std::vector<glm::vec4> cellPixels(cellCount.x * cellCount.y);

	for (uint32_t y = 0; y < cellCount.y; y++) {
		for (uint32_t x = 0; x < cellCount.x; x++) {
			uint32_t id = y * cellCount.x + x;

			float pressure = 0.0f;

			float temperature =
				293.15f +
				(((float)rand() / RAND_MAX) * 2.0f - 1.0f) * 40.0f;

			float density =
				1.225f +
				(((float)rand() / RAND_MAX) * 2.0f - 1.0f) * 0.2f;

			cellPixels[id] = glm::vec4(
				pressure,
				temperature,
				density,
				0.0f
			);
		}
	}

	std::vector<float> flowXPixels((cellCount.x + 1) * cellCount.y);
	std::vector<float> flowYPixels(cellCount.x * (cellCount.y + 1));

	for (float& v : flowXPixels) {
		v = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * 10.0f;
	}

	for (float& v : flowYPixels) {
		v = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * 10.0f;
	}

	for (int i = 0; i < 2; i++) {
		cellData[i].write(
			0, 0, 0,
			cellCount.x, cellCount.y,
			GL_RGBA,
			GL_FLOAT,
			cellPixels.data());

		flowX[i].write(
			0, 0, 0,
			cellCount.x + 1, cellCount.y,
			GL_RED,
			GL_FLOAT,
			flowXPixels.data());

		flowY[i].write(
			0, 0, 0,
			cellCount.x, cellCount.y + 1,
			GL_RED,
			GL_FLOAT,
			flowYPixels.data());
	}
}