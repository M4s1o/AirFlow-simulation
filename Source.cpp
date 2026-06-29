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

#include "fluid_grid.h"

void randomizeSimulation(
	Texture cellData[2],
	Texture flowX[2],
	Texture flowY[2],
	glm::ivec2 cellCount);

int main() {
	// ===========================
	// setup
	// ===========================

	srand((unsigned int)time(nullptr));

	// window setup
	glfwInit();
	Window window;
	//window.setVsync(false);
	//window.setResizable(false);
	window.setSize(0, 0, 800, 800);

	// ImGui setup
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForOpenGL(window.getContext(), true);
	ImGui_ImplOpenGL3_Init("#version 460");
	ImGui::StyleColorsDark();

	FluidGrid fluid_grid({ 8, 8 });

	while (!window.shouldClose()) {
		window.updateFormat();
		window.clear(GL_COLOR_BUFFER_BIT);
		window.setBackground(0.5, 0.1, 0.1, 1);
		window.setViewportPos(0, 0, 0, 0);
		window.setViewportSize(1, 1, 0, 0);
		window.setViewport();

		//fluid_grid.compute_velocity_advection();

		//std::vector<glm::vec4> pixels(cellData[current_data].getWidth() * cellData[current_data].getHeight());
		//
		//glGetTextureImage(
		//	cellData[current_data].getID(),
		//	0,
		//	GL_RGBA,
		//	GL_FLOAT,
		//	pixels.size() * sizeof(glm::vec4),
		//	pixels.data()
		//);

		fluid_grid.render_cells();

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("settings");
		ImGui::SliderFloat("delta time", &fluid_grid.dt, 0, 1.0f / 20.0f, "%.7f");
		ImGui::SliderFloat("sor weight", &fluid_grid.SOR, 1.0f, 2.0f, "%.3f");

		const char* items[] = { "divergence", "pressure"};
		ImGui::Combo("render mode", &fluid_grid.render_mode, items, 2);

		if (ImGui::TreeNode("flow arrows"))
		{
			ImGui::SliderFloat("arrow scale", &fluid_grid.arrow_scale, 0.0f, 0.3f, "%.3f");
			ImGui::SliderFloat("arrow value", &fluid_grid.arrow_value, 0.0f, 3.0f, "%.3f");
			static float vector_color[4] = {0.1, 0.1, 0.8, 1.0};
			ImGui::ColorEdit4("color", vector_color);

			fluid_grid.render_main_velocities({
				vector_color[0],
				vector_color[1],
				vector_color[2],
				vector_color[3]
			});

			ImGui::TreePop();
		}
		if (ImGui::Button("run pressure")) {
			fluid_grid.compute_pressure(1);
		}
		if (ImGui::Button("run divergence")) {
			fluid_grid.compute_divergence();
		}
		if (ImGui::Button("run velocities")) {
			fluid_grid.compute_velocities();
		}
		if (ImGui::Button("randomize")) {
			randomizeSimulation(fluid_grid.cellData, fluid_grid.flowX, fluid_grid.flowY, fluid_grid.getGridSize());
		}
		if (ImGui::Button("reset")) {
			ImGui::Text("not yet implemented");
		}
		if (ImGui::Button("experiment")) {
			float speed = 0.25f;
			fluid_grid.flowX[0].write(
				0, 4, 4,
				1, 1,
				GL_RED,
				GL_FLOAT,
				&speed);
			fluid_grid.flowX[1].write(
				0, 4, 4,
				1, 1,
				GL_RED,
				GL_FLOAT,
				&speed);
		}
		ImGui::End();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		window.swapBuffers();
		glfwPollEvents();
	}
	return 1;
}

void randomizeSimulation(
	Texture cellData[2],
	Texture flowX[2],
	Texture flowY[2],
	glm::ivec2 cellCount)
{
	std::vector<glm::vec4> cellPixels(cellCount.x * cellCount.y);

	for (uint32_t y = 0; y < cellCount.y; y++) {
		for (uint32_t x = 0; x < cellCount.x; x++) {
			uint32_t id = y * cellCount.x + x;

			float pressure = 0.0f;
				//(((float)rand() / RAND_MAX) * 2.0f - 1.0f);

			float temperature = 0.0f;
				//293.15f +
				//(((float)rand() / RAND_MAX) * 2.0f - 1.0f) * 40.0f;

			float density = 0.0f;
				//1.225f +
				//(((float)rand() / RAND_MAX) * 2.0f - 1.0f) * 0.2f;

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
		v = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * 0.25f;
	}

	for (float& v : flowYPixels) {
		v = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * 0.25f;
	}

	for (int y = 0; y < cellCount.y; y++) {
		flowXPixels[y * (cellCount.x + 1) + 0] = 0.0f;
		flowXPixels[y * (cellCount.x + 1) + cellCount.x] = 0.0f;
	}

	for (int x = 0; x < cellCount.x; x++) {
		flowYPixels[0 * cellCount.x + x] = 0.0f;
		flowYPixels[cellCount.y * cellCount.x + x] = 0.0f;
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