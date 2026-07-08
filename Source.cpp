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
	window.setSize(0, 0, 1920, 1080);
	window.setName("Air - flower");

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// ImGui setup
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForOpenGL(window.getContext(), true);
	ImGui_ImplOpenGL3_Init("#version 460");
	ImGui::StyleColorsDark();

	// time
	const auto start_time = std::chrono::steady_clock::now();
	auto current_time = start_time;
	auto last_frame_time = current_time;

	FluidGrid fluid_grid({ 1920 / 4, 1080 / 4 });

	// ==========================================
	// VARIABLE DEFINITIONS
	// ==========================================

	const char* paint_shapes[] = { "rectangle", "sphere" };
	const int paint_shapes_count = 2;
	int paint_shape = 0;

	bool equal_sides = false;
	float rectangle_dimensions[2] = { 0.1f, 0.1f };
	float circle_radius = 0.01f;

	bool esc_pressed = false;
	bool shift_pressed = false;
	bool space_pressed = false;
	bool num_1_pressed = false;
	bool num_2_pressed = false;
	bool num_3_pressed = false;
	bool num_4_pressed = false;
	bool tilde_pressed = false;
	bool tab_pressed = false;
	bool R_pressed = false;
	bool C_pressed = false;

	bool LMB_pressed = false;
	bool RMB_pressed = false;
	double cursor_X_position;
	double cursor_Y_position;

	bool render_grid_arrows = false;
	bool render_flow_arrows = false;
	bool render_obstacles = true;
	bool render_ui = true;

	float obstacle_color[4] = { 1.0, 1.0, 1.0, 1.0 };

	float grid_arrows_width = 0.17f;
	float grid_arrows_magnitude = 2.0f;
	float grid_arrows_color[4] = { 0.1, 0.1, 0.8, 1.0 };

	const char* render_modes[] = { "divergence", "pressure", "attribute", "flow" };
	const int render_modes_count = 4;
	int cell_render_mode = 0;
	bool continous_rendering = false;
	float color_maximum = 1.0f;

	bool manual_dt_control = false;
	float time_step = 0.0f;
	float simulation_speed = 0.0;
	float delta_time = 0.0f;
	bool paused = true;

	bool vsync = true;
	float SOR = 1.0f;
	int rbGS_iteration_count = 60;

	float density = 1.225f;

	const char* modify_actions[] = { "wall", "spawner", "stir", "attribute" };
	const int modify_actions_count = 4;
	int modifying_action = 0;

	// ==========================================
	// FUNCTION DEFINITIONS
	// ==========================================

	auto auto_config = [
		&fluid_grid,
		&manual_dt_control,
		&time_step,
		&simulation_speed,
		&paused,
		&cell_render_mode,
		&render_grid_arrows,
		&color_maximum,
		&rbGS_iteration_count,
		&SOR]() {

		manual_dt_control = true;
		time_step = 1.0f / 60.0f;
		simulation_speed = 1.0f;
		paused = false;
		cell_render_mode = 2;
		render_grid_arrows = false;
		color_maximum = 1.0f;
		rbGS_iteration_count = 30;
		SOR = 1.7f;
	};

	auto button_released = [&window](int glfw_button, bool &pressed_last_frame) {
		if (glfwGetKey(window.getContext(), glfw_button) == GLFW_PRESS) {
			if (!pressed_last_frame) {
				pressed_last_frame = true;
				return true;
			}
		}
		else pressed_last_frame = false;
		return false;
	};
	auto mouse_button_released = [&window](int glfw_button, bool &pressed_last_frame) {
		if (glfwGetMouseButton(window.getContext(), glfw_button) == GLFW_PRESS) {
			if (!pressed_last_frame) {
				pressed_last_frame = true;
				return true;
			}
		}
		else pressed_last_frame = false;
		return false;
	};

	// ==========================================
	// PROGRAM LOOP
	// ==========================================
	while (!window.shouldClose()) {
		window.updateFormat();
		window.clear(GL_COLOR_BUFFER_BIT);
		window.setBackground(0.1f, 0.1f, 0.1f, 1.0f);
		window.setViewportPos(0.0f, 0.0f, 0.0f, 0.0f);
		window.setViewportSize(1.0f, 1.0f, 0.0f, 0.0f);
		window.setViewport();

		// ==========================================
		// TIME CONTROL
		// ==========================================

		last_frame_time = current_time;
		current_time = std::chrono::steady_clock::now();

		if (manual_dt_control) {
			while (std::chrono::duration<float>(current_time - last_frame_time).count() < time_step) {
				current_time = std::chrono::steady_clock::now();
			}
		}
		else
			time_step = std::chrono::duration<float>(current_time - last_frame_time).count();

		delta_time = time_step * simulation_speed;

		// ==========================================
		// SIMULATION
		// ==========================================

		if (!paused && delta_time != 0) {
			fluid_grid.compute_divergence(delta_time, density);
			fluid_grid.compute_pressure(rbGS_iteration_count, SOR);
			fluid_grid.compute_velocities(delta_time, density);
			fluid_grid.compute_attribute_advection(delta_time);
			fluid_grid.compute_velocity_advection(delta_time);
			std::vector<float> speed(15, 5.0f);
			fluid_grid.velocity_X_tex()->write(
				0, 1, fluid_grid.getGridSize().y / 2 - 7,
				1, 15,
				GL_RED,
				GL_FLOAT,
				speed.data()
			);
			int n = 10;
			std::vector<float> attribute(4 * n, 2.0f);
			fluid_grid.attribute_tex()->write(
				0, 1, (fluid_grid.getGridSize().y - n) / 2,
				1, n,
				GL_RGBA,
				GL_FLOAT,
				attribute.data()
			);
		}

		// ==========================================
		// BUTTON INPUT
		// ==========================================
		if (button_released(GLFW_KEY_ESCAPE, esc_pressed))
			render_ui = !render_ui;

		if (button_released(GLFW_KEY_SPACE, space_pressed))
			paused = !paused;

		if (button_released(GLFW_KEY_1, num_2_pressed))
			cell_render_mode = 0;

		if (button_released(GLFW_KEY_2, num_2_pressed))
			cell_render_mode = 1;

		if (button_released(GLFW_KEY_3, num_3_pressed))
				cell_render_mode = 2;

		if (button_released(GLFW_KEY_4, num_4_pressed))
			cell_render_mode = 3;

		if (button_released(GLFW_KEY_GRAVE_ACCENT, tilde_pressed))
			render_obstacles = !render_obstacles;

		if (button_released(GLFW_KEY_C, C_pressed)) {
			auto_config();
		}

		if (button_released(GLFW_KEY_R, R_pressed))
			fluid_grid.reset();

		if (button_released(GLFW_KEY_TAB, tab_pressed)) {
			continous_rendering = !continous_rendering;
			GLenum filter = continous_rendering ? GL_LINEAR : GL_NEAREST;
			fluid_grid.pressure_tex()->setFilter(filter, filter);
			fluid_grid.divergence_tex()->setFilter(filter, filter);
			fluid_grid.attribute_tex()->setFilter(filter, filter);

			fluid_grid.divergence_tex_2()->setFilter(filter, filter);
			fluid_grid.attribute_tex_2()->setFilter(filter, filter);
		}

		// ==========================================
		// CELL RENDERING
		// ==========================================

		fluid_grid.render_cells(cell_render_mode, 1.0f / color_maximum);

		if (render_obstacles) {
			fluid_grid.render_obstacles({
				obstacle_color[0],
				obstacle_color[1],
				obstacle_color[2],
				obstacle_color[3]
			});
		}

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

		// ==========================================
		// UI RENDERING + INPUT
		// ==========================================
		if (render_ui) {
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			const float ui_width = 100.0f;
			ImGui::SetNextWindowSizeConstraints(ImVec2(210, FLT_MIN), ImVec2(FLT_MAX, FLT_MAX));
			ImGui::Begin("settings");

			// ==========================================
			// RENDERING
			// ==========================================
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
					ImGui::Combo("render mode", &cell_render_mode, render_modes, render_modes_count);

					ImGui::SetNextItemWidth(ui_width);
					ImGui::SliderFloat("max color", &color_maximum, 0.01f, 2.0f, "%.3f");

					ImGui::Checkbox(" render obstacles", &render_obstacles);

					if (ImGui::Checkbox(" continous rendering", &continous_rendering)) {
						GLenum filter = continous_rendering ? GL_LINEAR : GL_NEAREST;
						fluid_grid.pressure_tex()->setFilter(filter, filter);
						fluid_grid.divergence_tex()->setFilter(filter, filter);
						fluid_grid.attribute_tex()->setFilter(filter, filter);

						fluid_grid.divergence_tex_2()->setFilter(filter, filter);
						fluid_grid.attribute_tex_2()->setFilter(filter, filter);
					}

					ImGui::SetNextItemWidth(ui_width);
					ImGui::ColorEdit4("obstacle color", obstacle_color);

					ImGui::TreePop();
				}
				ImGui::TreePop();
			}

			// ==========================================
			// PHYSICS
			// ==========================================
			if (ImGui::TreeNode("simulation")) {
				ImGui::SeparatorText("time");

				ImGui::Checkbox(" manual time step", &manual_dt_control);
				if (manual_dt_control) {
					ImGui::SetNextItemWidth(ui_width);
					ImGui::SliderFloat("delta time", &time_step, 0, 1.0f / 20.0f, "%.7f");
				}

				ImGui::SetNextItemWidth(ui_width);
				ImGui::SliderFloat("sim speed", &simulation_speed, 0, 3.0f, "%.4f");

				ImGui::Checkbox(" pause", &paused);

				ImGui::SeparatorText("simulation");

				if (ImGui::Checkbox(" Vsync", &vsync))
					window.setVsync(vsync);

				ImGui::SetNextItemWidth(ui_width);
				ImGui::SliderFloat("sor weight", &SOR, 1.0f, 2.0f, "%.3f");

				ImGui::SetNextItemWidth(ui_width);
				ImGui::InputInt("iter count", &rbGS_iteration_count);

				ImGui::SeparatorText("physics");

				ImGui::SetNextItemWidth(ui_width);
				ImGui::SliderFloat("density", &density, 0.0f, 2.0f, "%.3f");

				ImGui::TreePop();
			}


			// ==========================================
			// MODIFY
			// ==========================================
			// TODO: interaction system
			if (ImGui::TreeNode("modify")) {
				ImGui::SetNextItemWidth(ui_width);
				ImGui::Combo("action", &modifying_action, modify_actions, modify_actions_count);

				switch (modifying_action) {
				case 0:
					ImGui::Text("LMB - paint wall");
					ImGui::Text("RMB - erase wall");

					ImGui::SetNextItemWidth(ui_width);
					ImGui::Combo("shape", &paint_shape, paint_shapes, paint_shapes_count);

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
						ImGui::SliderFloat("radius", &circle_radius, 0, 0.1f, "%.4f");

						if (!ImGui::GetIO().WantCaptureMouse && glfwGetMouseButton(window.getContext(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
							glfwGetCursorPos(window.getContext(), &cursor_X_position, &cursor_Y_position);

							float local_cursor_position_X = cursor_X_position / window.getFormat()->height;
							float local_cursor_position_Y = 1.0f - cursor_Y_position / window.getFormat()->height;

							fluid_grid.draw_circle(
								{ local_cursor_position_X, local_cursor_position_Y },
								circle_radius, 1);
						}
						if (!ImGui::GetIO().WantCaptureMouse && glfwGetMouseButton(window.getContext(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
							glfwGetCursorPos(window.getContext(), &cursor_X_position, &cursor_Y_position);

							float local_cursor_position_X = cursor_X_position / window.getFormat()->height;
							float local_cursor_position_Y = 1.0f - cursor_Y_position / window.getFormat()->height;

							fluid_grid.draw_circle(
								{ local_cursor_position_X, local_cursor_position_Y },
								circle_radius, 0);
						}
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
				if (ImGui::Button("reset grid"))
					fluid_grid.reset();

				ImGui::TreePop();
			}

			// ==========================================
			// CONTROLS
			// ==========================================
			if (ImGui::TreeNode("controls")) {

				ImGui::Text("Esc - toggle ui");
				ImGui::Text("Space - toggle paused");
				ImGui::Text("Tildo - toggle obstacles");
				ImGui::Text("Tab - continous rendering");
				ImGui::Text("1-4 - cycle render modes");
				ImGui::Text("R - reset grids");
				ImGui::Text("C - auto config");

				ImGui::TreePop();
			}

			// ==========================================
			// DEBUG
			// ==========================================
			if (ImGui::TreeNode("debug")) {
				ImGui::Text("fps: %f.2", 1.0f / std::chrono::duration<float>(current_time - last_frame_time).count());
				ImGui::Text("time step: %f.2", time_step);
				ImGui::Text("time step (real): %f.2", std::chrono::duration<float>(current_time - last_frame_time).count());
				ImGui::Text("delta time: %f.2", delta_time);

				ImGui::SetNextItemWidth(ui_width);
				if (ImGui::Button("reset grdids")) {
					fluid_grid.reset();
				}
				ImGui::SetNextItemWidth(ui_width);
				if (ImGui::Button("run divergence")) {
					fluid_grid.compute_divergence(delta_time, density);
				}

				ImGui::SetNextItemWidth(ui_width);
				if (ImGui::Button("run pressure")) {
					fluid_grid.compute_pressure(rbGS_iteration_count, SOR);
				}

				ImGui::SetNextItemWidth(ui_width);
				if (ImGui::Button("run velocities")) {
					fluid_grid.compute_velocities(delta_time, density);
				}

				ImGui::SetNextItemWidth(ui_width);
				if (ImGui::Button("run vel advection")) {
					fluid_grid.compute_velocity_advection(delta_time);
				}

				ImGui::SetNextItemWidth(ui_width);
				if (ImGui::Button("run one frame")) {
					fluid_grid.compute_divergence(delta_time, density);
					fluid_grid.compute_pressure(rbGS_iteration_count, SOR);
					fluid_grid.compute_velocities(delta_time, density);
					fluid_grid.compute_velocity_advection(delta_time);
				}
				ImGui::SetNextItemWidth(ui_width);
				if (ImGui::Button("auto set")) {
					auto_config();
				}
				ImGui::TreePop();
			}
			ImGui::End();

			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		}

		window.swapBuffers();
		glfwPollEvents();
	}
	return 1;
}