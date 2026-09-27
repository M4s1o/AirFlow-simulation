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
	window.setMaximized(true);
	window.setFullscreen(true);

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
	
	glm::ivec2 grid_resolution = { 640, 360 };
	FluidGrid fluid_grid(grid_resolution);

	// ==========================================
	// VARIABLE DEFINITIONS
	// ==========================================

	bool program_should_close = false;

	float max_velocity_on_grid = 0.0f;

	const char* demo_names[] = { "empty", "3 color jet", "smoke", "3 color fill", "2 jets", "wind tunnel", " 3 colored jets", "line of smoke" };
	const int demo_count = 6;
	int current_demo = 0;
	float specific_demo_velocity = 0.1f;

	const char* paint_shapes[] = { "rectangle", "circle", "line" };
	const int paint_shapes_count = 3;
	int paint_shape = 1;

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
	bool Q_pressed = false;
	bool F_pressed = false;
	bool F11_pressed = false;
			std::cout << "scheduled" << std::endl;

	bool LMB_pressed = false;
	bool RMB_pressed = false;
	double cursor_X_position;
	double cursor_Y_position;
	
	Fence cell_rendering_fence;
	Fence cell_compute_fence;
	bool render_now = false;
	bool compute_now = false;

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
	float injected_delta_time = 0.0f;
	bool paused = true;

	bool vsync = true;
	float SOR = 1.0f;
	int rbGS_iteration_count = 60;

	float density = 1.225f;

	const char* modify_actions[] = { "wall", "spawner", "stir", "attribute" };
	const int modify_actions_count = 4;
	int modifying_action = 0;

	auto last_complexity_detection = current_time;

	// ==========================================
	// FUNCTION DEFINITIONS
	// ==========================================

	auto auto_config = [
		&fluid_grid,
		&manual_dt_control,
		&injected_delta_time,
		&simulation_speed,
		&paused,
		&cell_render_mode,
		&render_grid_arrows,
		&color_maximum,
		&rbGS_iteration_count,
		&SOR,
		&current_demo,
		&window,
		&vsync]() {

		manual_dt_control = true;
		injected_delta_time = 0.0078125f;
		simulation_speed = 1.0f;
		paused = true;
		cell_render_mode = 2;
		render_grid_arrows = false;
		color_maximum = 1.0f;
		rbGS_iteration_count = 30;
		SOR = 1.7f;
		current_demo = 5;
		vsync = false;
		window.setVsync(vsync);
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
	while (!window.shouldClose() && !program_should_close) {
		// ==========================================
		// SCHEDULING
		// ==========================================
		
		render_now = false;
		compute_now = false;

		if (cell_rendering_fence.signaled() && cell_compute_fence.signaled()) {
			render_now = true;
			compute_now = true;
		}
		render_now = true;

		// ==========================================
		// TIME CONTROL
		// ==========================================

		if (!paused && compute_now) {
			last_frame_time = current_time;
			current_time = std::chrono::steady_clock::now();
			time_step = std::chrono::duration<float>(current_time - last_frame_time).count();

			if (manual_dt_control) {
				float expected_delta_time = time_step * simulation_speed;
				delta_time = std::min(injected_delta_time, expected_delta_time);
			}
			else {
				delta_time = time_step * simulation_speed;
			}
		}

		// ==========================================
		// UPDATE LOOP
		// ==========================================

		if (!paused && delta_time != 0 && compute_now) {
			max_velocity_on_grid = 999999999.9f;
			switch (current_demo) {
				case 1:
					{
					float specific_velocity = specific_demo_velocity;
					max_velocity_on_grid = specific_velocity;
					const int n = 60;
					fluid_grid.setVelocity_X(0, (fluid_grid.getGridSize().y - n) / 2, 1, n, specific_velocity);

					fluid_grid.setAttributes(0, (fluid_grid.getGridSize().y - n / 3) / 2 + n / 3, 1, n / 3, { 0.0f, 0.0f, 1.0f, 1.0f });
					fluid_grid.setAttributes(0, (fluid_grid.getGridSize().y - n / 3) / 2        , 1, n / 3, { 0.0f, 1.0f, 0.0f, 1.0f });
					fluid_grid.setAttributes(0, (fluid_grid.getGridSize().y - n / 3) / 2 - n / 3, 1, n / 3, { 1.0f, 0.0f, 0.0f, 1.0f });
					break;
					}
				case 2:
					{

					float specific_velocity = specific_demo_velocity;
					max_velocity_on_grid = specific_demo_velocity;
					const int n = 30;
					fluid_grid.setVelocity_Y((fluid_grid.getGridSize().x - n) / 2, 4, n, 1, specific_demo_velocity);
					fluid_grid.setAttributes((fluid_grid.getGridSize().x - n) / 2, 4, n, 1, { 1.0f, 1.0f, 1.0f, 1.0f });
					break;
					}
				case 3:
					{
					float specific_velocity = specific_demo_velocity;
					max_velocity_on_grid = specific_demo_velocity;
					const int n = 60;
					fluid_grid.setVelocity_X(0, (fluid_grid.getGridSize().y - n) / 2, 1, n, specific_demo_velocity);
					break;
					}
				case 4:
					{
					float specific_velocity = specific_demo_velocity;
					max_velocity_on_grid = specific_demo_velocity;
					const int n = 10;
					fluid_grid.setVelocity_X(2, (fluid_grid.getGridSize().y - n) / 2, 1, n, specific_demo_velocity);
					fluid_grid.setAttributes(1, (fluid_grid.getGridSize().y - n) / 2, 1, n, { 1.0f, 0.0f, 0.0f, 1.0f });

					fluid_grid.setVelocity_X(fluid_grid.getGridSize().x -2, (fluid_grid.getGridSize().y - n) / 2, 1, n, -specific_demo_velocity);
					fluid_grid.setAttributes(fluid_grid.getGridSize().x -2, (fluid_grid.getGridSize().y - n) / 2, 1, n, { 0.0f, 1.0f, 0.0f, 1.0f });
					break;
					}
				case 5:
					{
					float specific_velocity = specific_demo_velocity;
					max_velocity_on_grid = specific_demo_velocity;
					const int n = 60;
					fluid_grid.setVelocity_X(fluid_grid.getGridSize().x / 2, (fluid_grid.getGridSize().y - n) / 2, 2, n, specific_demo_velocity);
					fluid_grid.setAttributes(fluid_grid.getGridSize().x / 2, (fluid_grid.getGridSize().y - n) / 2, 2, n, {0.0f, 1.0f, 0.0f, 1.0f });
					break;
					}
			}

			fluid_grid.compute_divergence(delta_time, density);
			fluid_grid.compute_pressure(rbGS_iteration_count, SOR);

			//fluid_grid.setPressure(fluid_grid.getGridSize().x - 1, 0, 1, fluid_grid.getGridSize().y, 0.0f);
			//fluid_grid.setVelocity_X(fluid_grid.getGridSize().x - 1, 0, 1, fluid_grid.getGridSize().y, 10.0f);

			fluid_grid.compute_velocities(delta_time, density);
			fluid_grid.compute_attribute_advection(delta_time);window.setVsync(vsync);
			fluid_grid.compute_velocity_advection(delta_time);
			cell_compute_fence.place();
		}

		// ==========================================
		// BUTTON INPUT
		// ==========================================
		if (button_released(GLFW_KEY_F11, F11_pressed))
			window.setFullscreen(!window.getFormat()->fullscreen);

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

		if (button_released(GLFW_KEY_F, F_pressed)) {
			fluid_grid.reset_fluid();
			fluid_grid.reset_attributes();
		}

		if (button_released(GLFW_KEY_TAB, tab_pressed)) {
			continous_rendering = !continous_rendering;
			GLenum filter = continous_rendering ? GL_LINEAR : GL_NEAREST;
			fluid_grid.pressure_tex()->setFilter(filter, filter);
			fluid_grid.divergence_tex()->setFilter(filter, filter);
			fluid_grid.attribute_tex()->setFilter(filter, filter);

			fluid_grid.divergence_tex_2()->setFilter(filter, filter);
			fluid_grid.attribute_tex_2()->setFilter(filter, filter);
		}
		if (button_released(GLFW_KEY_Q, Q_pressed))
			program_should_close = true;

		// ==========================================
		// CELL RENDERING
		// ==========================================

		if (render_now) {
			window.updateFormat();
			window.clear(GL_COLOR_BUFFER_BIT);
			window.setBackground(0.1f, 0.1f, 0.1f, 1.0f);
			window.setViewportPos(0.0f, 0.0f, 0.0f, 0.0f);
			window.setViewportSize(1.0f, 1.0f, 0.0f, 0.0f);
			window.setViewport();

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
			cell_rendering_fence.place();
		}

		// ==========================================
		// UI RENDERING + INPUT
		// ==========================================
		if (render_now && render_ui) {
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

				// TODO: automatic stabilizer (adjusts delta_time based on courant number)

				ImGui::Checkbox(" manual time step", &manual_dt_control);
				if (manual_dt_control) {
					ImGui::SetNextItemWidth(ui_width);
					ImGui::SliderFloat("delta time", &injected_delta_time, 0, 1.0f / 20.0f, "%.7f");
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
						ImGui::SeparatorText("rectangle / square");

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
						// circle
						ImGui::SeparatorText("circle");

						ImGui::SetNextItemWidth(ui_width);
						ImGui::SliderFloat("radius", &circle_radius, 0, 0.1f, "%.4f");

						if (!ImGui::GetIO().WantCaptureMouse && glfwGetMouseButton(window.getContext(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
							glfwGetCursorPos(window.getContext(), &cursor_X_position, &cursor_Y_position);

							float local_cursor_position_X = cursor_X_position / (float)window.getFormat()->height;
							float local_cursor_position_Y = 1.0f - cursor_Y_position / (float)window.getFormat()->height;

							fluid_grid.draw_circle(
								{ local_cursor_position_X, local_cursor_position_Y },
								circle_radius, 1);
						}
						if (!ImGui::GetIO().WantCaptureMouse && glfwGetMouseButton(window.getContext(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
							glfwGetCursorPos(window.getContext(), &cursor_X_position, &cursor_Y_position);

							float local_cursor_position_X = cursor_X_position / (float)window.getFormat()->height;
							float local_cursor_position_Y = 1.0f - cursor_Y_position / (float)window.getFormat()->height;

							fluid_grid.draw_circle(
								{ local_cursor_position_X, local_cursor_position_Y },
								circle_radius, 0);
						}
						break;
					case 2:
						// line
						ImGui::SeparatorText("line");
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
				if (ImGui::Combo("demo", &current_demo, demo_names, demo_count)) {
					switch (current_demo) {
						case 0:
							break;
						case 1:
							break;
						case 2:
							break;
						case 3:
							fluid_grid.setAttributes(fluid_grid.getGridSize().x / 2 * 0 + 1, 1, fluid_grid.getGridSize().x / 3, fluid_grid.getGridSize().y, { 0.0f, 0.0f, 1.0f, 1.0f });
							fluid_grid.setAttributes(fluid_grid.getGridSize().x / 2 * 1 + 1, 1, fluid_grid.getGridSize().x / 3, fluid_grid.getGridSize().y, { 0.0f, 1.0f, 0.0f, 1.0f });
							fluid_grid.setAttributes(fluid_grid.getGridSize().x / 2 * 2 + 1, 1, fluid_grid.getGridSize().x / 3, fluid_grid.getGridSize().y, { 1.0f, 0.0f, 0.0f, 1.0f });
							break;
						case 4:
							break;
						case 5:
							break;
					}
				}

				ImGui::SetNextItemWidth(ui_width);
				ImGui::SliderFloat("demo velocity", &specific_demo_velocity, 0.01, 0.3, "%.3f");



				ImGui::SetNextItemWidth(ui_width);
				if (ImGui::Button("reset fluid")) {
					fluid_grid.reset_fluid();
					//fluid_grid.reset_attributes();
				}

				ImGui::SetNextItemWidth(ui_width);
				if (ImGui::Button("clear canvas"))
					fluid_grid.reset_obstacles();

				ImGui::SetNextItemWidth(ui_width);
				if (ImGui::Button("reset all"))
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
				ImGui::Text("Q - close program");

				ImGui::TreePop();
			}

			// ==========================================
			// DEBUG
			// ==========================================
			if (ImGui::TreeNode("debug")) {
				ImGui::Text("fps: %.1f", 1.0f / std::chrono::duration<float>(current_time - last_frame_time).count());
				ImGui::Text("time step: %f", time_step);
				ImGui::Text("time step (real): %f", std::chrono::duration<float>(current_time - last_frame_time).count());
				ImGui::Text("delta time: %f", delta_time);
				float real_time_speed = delta_time / std::chrono::duration<float>(current_time - last_frame_time).count();
				ImGui::Text("real-time  speed: %.6f", real_time_speed);
				ImGui::Text("simulation speed: %.6f", real_time_speed / simulation_speed);
				// TODO: find biggest velocity on the grid (max_velocity)
				/*
				float courant = max_velocity * delta_time / (1.0f / (float)grid_resolution.x);
				ImGui::Text("Courant number: %f.2", courant);
				if (courant < 0.3f)
					ImGui::Text("state: very precise");
				else if (courant < 0.5f)
					ImGui::Text("state: precise");
				else if (courant < 1.0f)
					ImGui::Text("state: stable (lossy)");
				else
					ImGui::Text("state: unstable");
				*/
				// TEMPORARY:
				ImGui::SeparatorText("stability");

				float courant = max_velocity_on_grid * delta_time / (1.0f / (float)grid_resolution.x);
				ImGui::Text("Courant number: %.2f", courant);

				if (courant < 0.3f)
					ImGui::Text("state: very precise");
				else if (courant < 0.5f)
					ImGui::Text("state: precise");
				else if (courant < 1.0f)
					ImGui::Text("state: stable (lossy)");
				else
					ImGui::Text("state: unstable");

				static float temp_requested_courant_number = 0.5f;
				ImGui::SetNextItemWidth(ui_width);
				ImGui::SliderFloat("req. courant", &temp_requested_courant_number, 0.0f, 1.0f, "%.2f");
				float ideal_delta_time = ((1.0f / (float)grid_resolution.x) * temp_requested_courant_number) / max_velocity_on_grid;
				ImGui::Text("ideal delta time: %f.3", ideal_delta_time);

				ImGui::SetNextItemWidth(ui_width);
				if (ImGui::Button("set ideal delta time")) {
					manual_dt_control = true;
					injected_delta_time = ideal_delta_time;
				}
				
				ImGui::SeparatorText("complexity / time distribution");

				ImGui::SetNextItemWidth(ui_width);
				if (ImGui::Button("detect time complexity")) {
					// TODO: detect time complexity for each step of the simulation
				}



				ImGui::SeparatorText("compute");

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

				ImGui::SetNextItemWidth(ui_width);
				if (ImGui::Button("create pressure wave"))
					fluid_grid.setVelocity_X(80, (fluid_grid.getGridSize().y - 41) / 2, 1, 41, 10.0f);

				ImGui::SetNextItemWidth(ui_width);
				if (ImGui::Button("set speed to 0.002"))
					simulation_speed = 0.002f;

				ImGui::TreePop();
			}

			ImGui::SetNextItemWidth(ui_width);
			if (ImGui::Button("CLOSE PROGRAM")) {
				program_should_close = true;
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
