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


	const char* paint_shapes[] = { "rectangle", "sphere" };
	static int paint_shape = 0;

	static bool equal_sides = false;
	static float rectangle_dimensions[2] = { 0.1f, 0.1f };
	static float sphere_radius = 0.1f;

	bool esc_pressed = false;
	bool shift_pressed = false;
	bool space_pressed = false;
	bool num_1_pressed = false;
	bool num_2_pressed = false;
	bool num_3_pressed = false;
	bool num_4_pressed = false;
	bool tilde_pressed = false;
	bool R_pressed = false;
	bool C_pressed = false;

	bool render_grid_arrows = false;
	bool render_flow_arrows = false;
	bool render_obstacles = true;
	bool render_ui = true;

	float obstacle_color[4] = { 1.0, 1.0, 1.0, 1.0 };

	float grid_arrows_width = 0.17f;
	float grid_arrows_magnitude = 2.0f;
	float grid_arrows_color[4] = { 0.1, 0.1, 0.8, 1.0 };

	const char* render_modes[] = { "divergence", "pressure", "attribute", "flow" };
	int cell_render_mode = 0;
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
	int modifying_action = 0;
	bool reset_all = false;

	int x = fluid_grid.obstacle_tex()->getWidth();
	int y = fluid_grid.obstacle_tex()->getHeight();
	std::vector<int> zeroed(x * y, 0);
	fluid_grid.obstacle_tex()->write(
		0, 0, 0,
		x, y,
		GL_RED_INTEGER,
		GL_UNSIGNED_BYTE,
		zeroed.data()
	);

	std::vector<int> obstacles(20, 1);
	obstacles.at(9) = 0;
	obstacles.at(10) = 0;
	obstacles.at(11) = 0;
	fluid_grid.obstacle_tex()->write(
		0, 100, fluid_grid.getGridSize().y / 2 - 10,
		1, 20,
		GL_RED_INTEGER,
		GL_UNSIGNED_BYTE,
		obstacles.data()
	);

	while (!window.shouldClose()) {
		window.updateFormat();
		window.clear(GL_COLOR_BUFFER_BIT);
		window.setBackground(0.1, 0.1, 0.1, 1);
		window.setViewportPos(0.025, 0.025, 0, 0);
		window.setViewportSize(0.95, 0.95, 0, 0);
		window.setViewport();

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

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

		if (glfwGetKey(window.getContext(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
			if (!esc_pressed) {
				esc_pressed = true;
				render_ui = !render_ui;
			}
		} else esc_pressed = false;

		if (glfwGetKey(window.getContext(), GLFW_KEY_SPACE) == GLFW_PRESS) {
			if (!space_pressed) {
				space_pressed = true;
				paused = !paused;
			}
		} else space_pressed = false;

		if (glfwGetKey(window.getContext(), GLFW_KEY_1) == GLFW_PRESS) {
			if (!num_1_pressed) {
				num_1_pressed = true;
				cell_render_mode = 0;
			}
		} else num_1_pressed = false;

		if (glfwGetKey(window.getContext(), GLFW_KEY_2) == GLFW_PRESS) {
			if (!num_2_pressed) {
				num_2_pressed = true;
				cell_render_mode = 1;
			}
		} else num_2_pressed = false;

		if (glfwGetKey(window.getContext(), GLFW_KEY_3) == GLFW_PRESS) {
			if (!num_3_pressed) {
				num_3_pressed = true;
				cell_render_mode = 2;
			}
		} else num_3_pressed = false;

		if (glfwGetKey(window.getContext(), GLFW_KEY_4) == GLFW_PRESS) {
			if (!num_4_pressed) {
				num_4_pressed = true;
				cell_render_mode = 3;
			}
		} else num_4_pressed = false;

		if (glfwGetKey(window.getContext(), GLFW_KEY_GRAVE_ACCENT) == GLFW_PRESS) {
			if (!tilde_pressed) {
				tilde_pressed = true;
				render_obstacles = !render_obstacles;
			}
		} else tilde_pressed = false;

		if (glfwGetKey(window.getContext(), GLFW_KEY_C) == GLFW_PRESS) {
			if (!C_pressed) {
				C_pressed = true;
				manual_dt_control = true;
				time_step = 1.0f / 60.0f;
				simulation_speed = 1.0f;
				paused = false;
				cell_render_mode = 2;
				render_grid_arrows = false;
				color_maximum = 1.0f;
				rbGS_iteration_count = 30;
				SOR = 1.7f;

				std::vector<int> obstacles(100, 1);
				int n = 10;
				for (int i = 0; i < n; i++) {
					obstacles.at(i + (100 - n) / 2) = 0;
				}
				fluid_grid.obstacle_tex()->write(
					0, 110, fluid_grid.getGridSize().y / 2 - 100,
					1, 100,
					GL_RED_INTEGER,
					GL_UNSIGNED_BYTE,
					obstacles.data()
				);
			}
		} else C_pressed = false;

		if (glfwGetKey(window.getContext(), GLFW_KEY_R) == GLFW_PRESS) {
			if (!R_pressed) {
				R_pressed = true;
				int x = fluid_grid.obstacle_tex()->getWidth();
				int y = fluid_grid.obstacle_tex()->getHeight();

				std::vector<int> zeroedObstacle(x * y, 0);
				std::vector<float> zeroedFlowX((x + 1) * y, 0.0f);
				std::vector<float> zeroedFlowY(x * (y + 1), 0.0f);
				std::vector<float> zeroedPressure(x * y, 0.0f);
				std::vector<float> zeroedDivergence(x * y * 2, 0.0f);
				std::vector<float> zeroedAttributes(x * y * 4, 0.0f);

				fluid_grid.obstacle_tex()->write(
					0, 0, 0,
					x, y,
					GL_RED_INTEGER,
					GL_UNSIGNED_BYTE,
					zeroedObstacle.data()
				);
				fluid_grid.velocity_X_tex()->write(
					0, 0, 0,
					x + 1, y,
					GL_RED,
					GL_FLOAT,
					zeroedFlowX.data()
				);
				fluid_grid.velocity_Y_tex()->write(
					0, 0, 0,
					x, y + 1,
					GL_RED,
					GL_FLOAT,
					zeroedFlowY.data()
				);
				fluid_grid.pressure_tex()->write(
					0, 0, 0,
					x, y,
					GL_RED,
					GL_FLOAT,
					zeroedPressure.data()
				);
				fluid_grid.divergence_tex()->write(
					0, 0, 0,
					x, y,
					GL_RG,
					GL_FLOAT,
					zeroedDivergence.data()
				);
				fluid_grid.attribute_tex()->write(
					0, 0, 0,
					x, y,
					GL_RGBA,
					GL_FLOAT,
					zeroedAttributes.data()
				);
			}
		} else R_pressed = false;

		if (glfwGetKey(window.getContext(), GLFW_KEY_GRAVE_ACCENT) == GLFW_PRESS) {
			if (!tilde_pressed) {
			tilde_pressed = true;
			render_obstacles = !render_obstacles;
			}
		} else tilde_pressed = false;

		if (render_ui) {
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
					ImGui::Combo("render mode", &cell_render_mode, render_modes, 4);

					ImGui::SetNextItemWidth(ui_width);
					ImGui::SliderFloat("intensity", &color_maximum, 0.1f, 2.0f, "%.3f");

					ImGui::Checkbox(" render obstacles", &render_obstacles);

					ImGui::SetNextItemWidth(ui_width);
					ImGui::ColorEdit4("obstacle color", obstacle_color);

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

			if (ImGui::TreeNode("controls")) {

				ImGui::Text("Esc - toggle ui");
				ImGui::Text("Space - toggle paused");
				ImGui::Text("1-4 - cycle render modes");
				ImGui::Text("Shift - speed up");
				ImGui::Text("Ctrl - slow down");
				ImGui::Text("Tildo - toggle obstacles");
				ImGui::Text("R - reset grids");
				ImGui::Text("C - auto config");

				ImGui::TreePop();
			}

			if (ImGui::TreeNode("debug")) {
				ImGui::Text("fps: %f.2", 1.0f / std::chrono::duration<float>(current_time - last_frame_time).count());
				ImGui::Text("time step: %f.2", time_step);
				ImGui::Text("time step (real): %f.2", std::chrono::duration<float>(current_time - last_frame_time).count());
				ImGui::Text("delta time: %f.2", delta_time);

				ImGui::SetNextItemWidth(ui_width);
				if (ImGui::Button("reset grdids")) {
					int x = fluid_grid.obstacle_tex()->getWidth();
					int y = fluid_grid.obstacle_tex()->getHeight();

					std::vector<int> zeroedObstacle(x * y, 0);
					std::vector<float> zeroedFlowX((x + 1) * y, 0.0f);
					std::vector<float> zeroedFlowY(x * (y + 1), 0.0f);
					std::vector<float> zeroedPressure(x * y, 0.0f);
					std::vector<float> zeroedDivergence(x * y * 2, 0.0f);
					std::vector<float> zeroedAttributes(x * y * 4, 0.0f);

					fluid_grid.obstacle_tex()->write(
						0, 0, 0,
						x, y,
						GL_RED_INTEGER,
						GL_UNSIGNED_BYTE,
						zeroedObstacle.data()
					);
					fluid_grid.velocity_X_tex()->write(
						0, 0, 0,
						x + 1, y,
						GL_RED,
						GL_FLOAT,
						zeroedFlowX.data()
					);
					fluid_grid.velocity_Y_tex()->write(
						0, 0, 0,
						x, y + 1,
						GL_RED,
						GL_FLOAT,
						zeroedFlowY.data()
					);
					fluid_grid.pressure_tex()->write(
						0, 0, 0,
						x, y,
						GL_RED,
						GL_FLOAT,
						zeroedPressure.data()
					);
					fluid_grid.divergence_tex()->write(
						0, 0, 0,
						x, y,
						GL_RG,
						GL_FLOAT,
						zeroedDivergence.data()
					);
					fluid_grid.attribute_tex()->write(
						0, 0, 0,
						x, y,
						GL_RGBA,
						GL_FLOAT,
						zeroedAttributes.data()
					);
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
				if (ImGui::Button("create divergence")) {
					float speed = 0.5f;
					fluid_grid.velocity_X_tex()->write(
						0, 1, fluid_grid.getGridSize().y / 2,
						1, 1,
						GL_RED,
						GL_FLOAT,
						&speed);
				}
				ImGui::SetNextItemWidth(ui_width);
				if (ImGui::Button("auto set")) {
					manual_dt_control = true;
					time_step = 1.0f / 60.0f;
					simulation_speed = 1.0f;
					paused = false;
					cell_render_mode = 2;
					render_grid_arrows = false;
					color_maximum = 1.0f;
					rbGS_iteration_count = 30;
					SOR = 1.7f;

					std::vector<int> obstacles(100, 1);
					int n = 10;
					for (int i = 0; i < n; i++) {
						obstacles.at(i + (100 - n) / 2) = 0;
					}
					fluid_grid.obstacle_tex()->write(
						0, 110, fluid_grid.getGridSize().y / 2 - 100,
						1, 100,
						GL_RED_INTEGER,
						GL_UNSIGNED_BYTE,
						obstacles.data()
					);
				}
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
			ImGui::End();
		}
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		window.swapBuffers();
		glfwPollEvents();
	}
	return 1;
}