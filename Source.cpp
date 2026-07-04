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
	window.setName("Air - flower");

	// ImGui setup
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForOpenGL(window.getContext(), true);
	ImGui_ImplOpenGL3_Init("#version 460");
	ImGui::StyleColorsDark();

	FluidGrid fluid_grid({ 20, 20 });


	const char* paint_shapes[] = { "rectangle", "sphere" };
	static int paint_shape = 0;

	static bool equal_sides = false;
	static float rectangle_dimensions[2] = { 0.1f, 0.1f };
	static float sphere_radius = 0.1f;


	bool render_grid_arrows = false;
	bool render_flow_arrows = false;

	float grid_arrows_width = 0.17f;
	float grid_arrows_magnitude = 2.0f;
	float grid_arrows_color[4] = { 0.1, 0.1, 0.8, 1.0 };

	const char* render_modes[] = { "divergence", "pressure" };
	int cell_render_mode = 0;
	float color_intensity = 1.0f;

	bool manual_dt_control = false;
	float time_step = 0.0f;
	float simulation_speed = 0.0;
	float delta_time = 0.0f;

	float SOR = 1.0f;
	int rbGS_iteration_count = 60;

	float density = 1.225f;

	const char* modify_actions[] = { "wall", "spawner", "stir", "attribute" };
	int modifying_action = 0;
	bool reset_all = false;

	while (!window.shouldClose()) {
		window.updateFormat();
		window.clear(GL_COLOR_BUFFER_BIT);
		window.setBackground(0.1, 0.1, 0.1, 1);
		window.setViewportPos(0.025, 0.025, 0, 0);
		window.setViewportSize(0.95, 0.95, 0, 0);
		window.setViewport();

		delta_time = time_step * simulation_speed;

		if (delta_time != 0) {
			fluid_grid.compute_divergence(delta_time, density);
			fluid_grid.compute_pressure(rbGS_iteration_count, SOR);
			fluid_grid.compute_velocities(delta_time, density);
			fluid_grid.compute_velocity_advection(delta_time);
		}

		fluid_grid.render_cells(cell_render_mode, color_intensity);

		if (render_grid_arrows) {
			fluid_grid.render_main_velocities(
				grid_arrows_width,
				grid_arrows_magnitude, {
				grid_arrows_color[0],
				grid_arrows_color[1],
				grid_arrows_color[2],
				grid_arrows_color[3]
				});
		}
		if (render_flow_arrows) {

		}

		std::vector<glm::vec4> pixels(fluid_grid.cellData.getWidth() * fluid_grid.cellData.getHeight());
		
		glGetTextureImage(
			fluid_grid.cellData.getID(),
			0,
			GL_RGBA,
			GL_FLOAT,
			pixels.size() * sizeof(glm::vec4),
			pixels.data()
		);

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		const float ui_width = 100.0f;
		ImGui::SetNextWindowSizeConstraints(ImVec2(210, FLT_MIN), ImVec2(FLT_MAX, FLT_MAX));
		ImGui::Begin("settings");

		if (ImGui::TreeNode("rendering")) {
			if (ImGui::TreeNode("flow"))
			{
				ImGui::SeparatorText("grid arrows");

				ImGui::Checkbox(" render##render_grid_arrows", &render_grid_arrows);

				ImGui::SetNextItemWidth(ui_width);
				ImGui::SliderFloat("width", &grid_arrows_width, 0.0f, 0.3f, "%.3f");

				ImGui::SetNextItemWidth(ui_width);
				ImGui::SliderFloat("magnitude", &grid_arrows_magnitude, 0.0f, 3.0f, "%.3f");

				ImGui::SetNextItemWidth(ui_width);
				ImGui::ColorEdit4("color", grid_arrows_color);

				ImGui::SeparatorText("flow arrows");

				ImGui::Checkbox(" render##render_flow_arrows", &render_flow_arrows);

				ImGui::TreePop();
			}

			if (ImGui::TreeNode("cells"))
			{
				ImGui::SetNextItemWidth(ui_width);
				ImGui::Combo("render mode", &cell_render_mode, render_modes, 2);

				ImGui::SetNextItemWidth(ui_width);
				ImGui::SliderFloat("intensity", &color_intensity, 0.1f, 32.0f, "%.3f");

				//fluid_grid.render_cells();

				ImGui::TreePop();
			}
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("simulation")) {
			ImGui::SeparatorText("time");

			ImGui::Checkbox(" manual time step", &manual_dt_control);
			if (manual_dt_control) {
				ImGui::SetNextItemWidth(ui_width);
				ImGui::SliderFloat("delta time", &time_step, 0, 1.0f / 20.0f, "%.7f");
			}

			ImGui::SetNextItemWidth(ui_width);
			ImGui::SliderFloat("sim speed", &simulation_speed, 0, 3.0f, "%.4f");

			ImGui::SeparatorText("simulation");

			ImGui::SetNextItemWidth(ui_width);
			ImGui::SliderFloat("sor weight", &SOR, 1.0f, 2.0f, "%.3f");

			ImGui::SetNextItemWidth(ui_width);
			ImGui::InputInt("iter count", &rbGS_iteration_count);

			ImGui::SeparatorText("physics");

			ImGui::SetNextItemWidth(ui_width);
			ImGui::SliderFloat("density", &density, 0.0f, 2.0f, "%.3f");

			ImGui::TreePop();
		}

		// TODO: move variables above main loop
		// TODO: interaction system
		if (ImGui::TreeNode("modify")) {
			ImGui::SetNextItemWidth(ui_width);
			ImGui::Combo("action", &modifying_action, modify_actions, 4);

			switch (modifying_action) {
			case 0:
				ImGui::Text("LMB: paint wall");
				ImGui::Text("RMB: erase wall");

				ImGui::SetNextItemWidth(ui_width);
				ImGui::Combo("shape", &paint_shape, paint_shapes, 2);

				switch (paint_shape) {
				case 0:
					// rectangle
					ImGui::Checkbox(" equal sides", &equal_sides);

					if (equal_sides) {
						// square
						ImGui::SetNextItemWidth(ui_width);
						ImGui::SliderFloat("side length", &rectangle_dimensions[0], 0, 2.0f, "%.4f");
						rectangle_dimensions[1] = rectangle_dimensions[0];
					}
					else {
						// rectangle
						ImGui::SetNextItemWidth(ui_width);
						ImGui::SliderFloat2("side lengths", rectangle_dimensions, 0, 2.0f, "%.4f");
					}
					break;
				case 1:
					// sphere
					ImGui::SetNextItemWidth(ui_width);
					ImGui::SliderFloat("radius", &sphere_radius, 0, 2.0f, "%.4f");
					break;
				}
				break;

			case 1:
				// spawner
				break;
			case 2:
				// stir
				break;
			case 3:
				// attribute
				break;
			}

			ImGui::SetNextItemWidth(ui_width);
			reset_all = ImGui::Button("reset grid");

			ImGui::TreePop();
		}


		//static bool run = false;
		//if (ImGui::Button("run")) {
		//	run = !run;
		//}
		//if (run) {
		//	fluid_grid.compute_divergence();
		//	fluid_grid.compute_pressure(60);
		//	fluid_grid.compute_velocities();
		//	fluid_grid.compute_velocity_advection();
		//	float speed = 10.0f;
		//	fluid_grid.flowX[fluid_grid.current_flow_data].write(
		//		0, 50, 100,
		//		1, 1,
		//		GL_RED,
		//		GL_FLOAT,
		//		&speed);
		//}
		//if (ImGui::Button("reset")) {
		//	ImGui::Text("not yet implemented");
		//}
		if (ImGui::Button("experiment")) {
			float speed = 0.5f;
			fluid_grid.flowX[0].write(
				0, 1, fluid_grid.getGridSize().y / 2,
				1, 1,
				GL_RED,
				GL_FLOAT,
				&speed);
			fluid_grid.flowX[1].write(
				0, 1, fluid_grid.getGridSize().y / 2,
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