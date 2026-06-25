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

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

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

	// simulation setup
	glm::ivec2 cell_count = { 8, 8 };
	glm::vec2 sim_size = { 2, 2 }; // meters
	float dt = 0.0f;
	float SOR = 0.3f;

	glm::bvec4 wall_status = { true, true, true, true };

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

	// shaders setup
	std::string shadersDirectory = "shaders/";

	ShaderProgram cellRenderProg;
	cellRenderProg.addShader(GL_VERTEX_SHADER, readFileToString(shadersDirectory + "rendering/render_cell.vert"));
	cellRenderProg.addShader(GL_FRAGMENT_SHADER, readFileToString(shadersDirectory + "rendering/render_cell.frag"));
	cellRenderProg.compile();


	ShaderProgram pressureComputeProg;
	pressureComputeProg.addShader(GL_COMPUTE_SHADER, readFileToString(shadersDirectory + "compute/pressure_solver.comp"));
	pressureComputeProg.compile();

	ShaderProgram velocityXComputeProg;
	velocityXComputeProg.addShader(GL_COMPUTE_SHADER, readFileToString(shadersDirectory + "compute/velocity_X_solver.comp"));
	velocityXComputeProg.compile();

	ShaderProgram velocityYComputeProg;
	velocityYComputeProg.addShader(GL_COMPUTE_SHADER, readFileToString(shadersDirectory + "compute/velocity_Y_solver.comp"));
	velocityYComputeProg.compile();

	// draw setup
	VAO cellVao;
	VAO flowVao;

	int width, height, channels;
	stbi_set_flip_vertically_on_load(true);
	unsigned char* vectorArrowPNG = stbi_load("vector_arrow.png", &width, &height, &channels, 4);

	Texture vectorTexture(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);
	vectorTexture.setFilter(GL_NEAREST, GL_NEAREST);
	vectorTexture.setWrap(GL_REPEAT, GL_REPEAT);

	vectorTexture.write(0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, vectorArrowPNG);

	stbi_image_free(vectorArrowPNG);

	vectorTexture.loadTexture();

	// simulation data setup
	Texture cellData[2] = {
		Texture(GL_TEXTURE_2D, 1, GL_RGBA16F, cell_count.x, cell_count.y),
		Texture(GL_TEXTURE_2D, 1, GL_RGBA16F, cell_count.x, cell_count.y)
	};
	Texture flowX[2] = {
		Texture(GL_TEXTURE_2D, 1, GL_R16F, cell_count.x + 1, cell_count.y),
		Texture(GL_TEXTURE_2D, 1, GL_R16F, cell_count.x + 1, cell_count.y)
	};
	Texture flowY[2] = {
		Texture(GL_TEXTURE_2D, 1, GL_R16F, cell_count.x, cell_count.y + 1),
		Texture(GL_TEXTURE_2D, 1, GL_R16F, cell_count.x, cell_count.y + 1)
	};
	bool current_data = false;
	bool current_flow_data = false;

	for (int i  = 0; i < 2; i++) {

		cellData[i].setFilter(GL_NEAREST, GL_NEAREST);
		flowX[i].setFilter(GL_NEAREST, GL_NEAREST);
		flowY[i].setFilter(GL_NEAREST, GL_NEAREST);

		cellData[i].setWrap(GL_REPEAT, GL_REPEAT);
		flowX[i].setWrap(GL_REPEAT, GL_REPEAT);
		flowY[i].setWrap(GL_REPEAT, GL_REPEAT);

		cellData[i].loadImage(GL_READ_WRITE, 0, GL_FALSE, 0, GL_RGBA16F);
		flowX[i].loadImage(GL_READ_WRITE, 0, GL_FALSE, 0, GL_R16F);
		flowY[i].loadImage(GL_READ_WRITE, 0, GL_FALSE, 0, GL_R16F);

		cellData[i].loadTexture();
		flowX[i].loadTexture();
		flowY[i].loadTexture();
	}

	// loop setup
	Fence fence;

	int frame_count = 0;
	while (!window.shouldClose()) {
		window.updateFormat();
		window.clear(GL_COLOR_BUFFER_BIT);
		window.setBackground(0.5, 0.1, 0.1, 1);
		window.setViewportPos(0, 0, 0, 0);
		window.setViewportSize(1, 1, 0, 0);
		window.setViewport();

		fence.wait(1000000000);

		// cell compute
		for (int i = 0; i < 1000; i++) {
			pressureComputeProg.useProgram();

			glUniformHandleui64ARB(glGetUniformLocation(pressureComputeProg.getID(), "cellData_Texture_in"), cellData[current_data].getImageHandle());
			glUniformHandleui64ARB(glGetUniformLocation(pressureComputeProg.getID(), "flowX_Texture_in"), flowX[current_flow_data].getImageHandle());
			glUniformHandleui64ARB(glGetUniformLocation(pressureComputeProg.getID(), "flowY_Texture_in"), flowY[current_flow_data].getImageHandle());

			glUniformHandleui64ARB(glGetUniformLocation(pressureComputeProg.getID(), "cellData_Texture_out"), cellData[!current_data].getImageHandle());

			glUniform2ui(glGetUniformLocation(pressureComputeProg.getID(), "cell_count"), cell_count.x, cell_count.y);
			glUniform1f(glGetUniformLocation(pressureComputeProg.getID(), "cell_side_lenght"), 1.0f / cell_count.y);
			glUniform1f(glGetUniformLocation(pressureComputeProg.getID(), "dt"), dt);
			glUniform1f(glGetUniformLocation(pressureComputeProg.getID(), "density"), 1.225f);

			pressureComputeProg.runCompute(cell_count.x, cell_count.y, 1, GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			current_data = !current_data;
		}

		// velocity X compute
		velocityXComputeProg.useProgram();
		
		glUniformHandleui64ARB(glGetUniformLocation(velocityXComputeProg.getID(), "cellData_Texture_in"), cellData[current_data].getImageHandle());
		glUniformHandleui64ARB(glGetUniformLocation(velocityXComputeProg.getID(), "flowX_Texture_in"), flowX[current_flow_data].getImageHandle());
		
		glUniformHandleui64ARB(glGetUniformLocation(velocityXComputeProg.getID(), "flowX_Texture_out"), flowX[!current_flow_data].getImageHandle());
		
		glUniform2ui(glGetUniformLocation(velocityXComputeProg.getID(), "cell_count"), cell_count.x, cell_count.y);
		glUniform1f(glGetUniformLocation(velocityXComputeProg.getID(), "cell_side_lenght"), 1.0f / cell_count.y);
		glUniform1f(glGetUniformLocation(velocityXComputeProg.getID(), "dt"), dt);
		glUniform1f(glGetUniformLocation(velocityXComputeProg.getID(), "density"), 1.225f);
		
		velocityXComputeProg.runCompute(cell_count.x + 1, cell_count.y, 1, GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		
		// velocity Y compute
		velocityYComputeProg.useProgram();
		
		glUniformHandleui64ARB(glGetUniformLocation(velocityYComputeProg.getID(), "cellData_Texture_in"), cellData[current_data].getImageHandle());
		glUniformHandleui64ARB(glGetUniformLocation(velocityYComputeProg.getID(), "flowY_Texture_in"), flowY[current_flow_data].getImageHandle());
		
		glUniformHandleui64ARB(glGetUniformLocation(velocityYComputeProg.getID(), "flowY_Texture_out"), flowY[!current_flow_data].getImageHandle());
		
		glUniform2ui(glGetUniformLocation(velocityYComputeProg.getID(), "cell_count"), cell_count.x, cell_count.y);
		glUniform1f(glGetUniformLocation(velocityYComputeProg.getID(), "cell_side_lenght"), 1.0f / cell_count.y);
		glUniform1f(glGetUniformLocation(velocityYComputeProg.getID(), "dt"), dt);
		glUniform1f(glGetUniformLocation(velocityYComputeProg.getID(), "density"), 1.225f);
		
		velocityYComputeProg.runCompute(cell_count.x, cell_count.y + 1, 1, GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		
		current_flow_data = !current_flow_data;

		// render
		cellRenderProg.useProgram();

		glUniformHandleui64ARB(glGetUniformLocation(cellRenderProg.getID(), "cell_texture"), cellData[current_data].getTextureHandle());
		glUniformHandleui64ARB(glGetUniformLocation(cellRenderProg.getID(), "flowX_texture"), flowX[current_flow_data].getTextureHandle());
		glUniformHandleui64ARB(glGetUniformLocation(cellRenderProg.getID(), "flowY_texture"), flowY[current_flow_data].getTextureHandle());

		glUniform2ui(glGetUniformLocation(cellRenderProg.getID(), "cell_count"), cell_count.x, cell_count.y);
		glUniform2i(glGetUniformLocation(cellRenderProg.getID(), "resolution"), window.getFormat()->width, window.getFormat()->height);

		cellVao.draw(GL_TRIANGLES, 6);

		std::vector<glm::vec4> pixels(cellData[current_data].getWidth() * cellData[current_data].getHeight());

		glGetTextureImage(
			cellData[current_data].getID(),
			0,
			GL_RGBA,
			GL_FLOAT,
			pixels.size() * sizeof(glm::vec4),
			pixels.data()
		);

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("settings");
		ImGui::SliderFloat("delta time", &dt, 0, 1.0f / 20.0f, "%.7f");
		if (ImGui::Button("randomize")) {
			randomizeSimulation(cellData, flowX, flowY, cell_count);
		}
		ImGui::SliderFloat("over relaxation", &SOR, 0.0f, 2.0f, "%.2f");
		if (ImGui::Button("begin test")) {
			frame_count = 0;
			dt = 0.0007;
		}
		ImGui::Text("frame: %i", frame_count);
		frame_count++;
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
	glm::ivec2 cellCount)
{
	std::vector<glm::vec4> cellPixels(cellCount.x * cellCount.y);

	for (uint32_t y = 0; y < cellCount.y; y++) {
		for (uint32_t x = 0; x < cellCount.x; x++) {
			uint32_t id = y * cellCount.x + x;

			float pressure = 0.5f;

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
		v = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * 0.5f;
	}

	for (float& v : flowYPixels) {
		v = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * 0.5f;
	}

	// Zero border faces of flowX (x=0 and x=cell_count.x columns)
	for (int y = 0; y < cellCount.y; y++) {
		flowXPixels[y * (cellCount.x + 1) + 0] = 0.0f; // left border
		flowXPixels[y * (cellCount.x + 1) + cellCount.x] = 0.0f; // right border
	}

	// Zero border faces of flowY (y=0 and y=cell_count.y rows)
	for (int x = 0; x < cellCount.x; x++) {
		flowYPixels[0 * cellCount.x + x] = 0.0f; // bottom border
		flowYPixels[cellCount.y * cellCount.x + x] = 0.0f; // top border
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