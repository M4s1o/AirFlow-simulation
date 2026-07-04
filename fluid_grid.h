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

enum ViewModes {
	divergence_view = 0,
	pressure_view = 1
};

class FluidGrid {
private:
	ShaderProgram cellRenderProg;
	ShaderProgram flowXrenderProg;
	ShaderProgram flowYrenderProg;
	ShaderProgram divergenceComputeProg;
	ShaderProgram pressureComputeProg;
	ShaderProgram velocityXComputeProg;
	ShaderProgram velocityYComputeProg;
	ShaderProgram advectionXComputeProg;
	ShaderProgram advectionYComputeProg;
	VAO vao;

	glm::ivec2 cell_count = { 6, 6 };
public:
	Texture cellData = {
		Texture(GL_TEXTURE_2D, 1, GL_RGBA32F, cell_count.x, cell_count.y)
	};
	Texture divergenceData[2] = {
		Texture(GL_TEXTURE_2D, 1, GL_RG32F, cell_count.x, cell_count.y),
		Texture(GL_TEXTURE_2D, 1, GL_RG32F, cell_count.x, cell_count.y)
	};
	Texture flowX[2] = {
		Texture(GL_TEXTURE_2D, 1, GL_R16F, cell_count.x + 1, cell_count.y),
		Texture(GL_TEXTURE_2D, 1, GL_R16F, cell_count.x + 1, cell_count.y)
	};
	Texture flowY[2] = {
		Texture(GL_TEXTURE_2D, 1, GL_R16F, cell_count.x, cell_count.y + 1),
		Texture(GL_TEXTURE_2D, 1, GL_R16F, cell_count.x, cell_count.y + 1)
	};

	bool current_flow_data = false;
	bool current_divergence_data = false;

	FluidGrid(glm::vec2 cell_count)
		: cell_count(cell_count) {
		std::string shadersDirectory = "shaders/";

		cellRenderProg.addShader(GL_VERTEX_SHADER, readFileToString(shadersDirectory + "rendering/render_cell.vert"));
		cellRenderProg.addShader(GL_FRAGMENT_SHADER, readFileToString(shadersDirectory + "rendering/render_cell.frag"));
		cellRenderProg.compile();

		flowXrenderProg.addShader(GL_VERTEX_SHADER, readFileToString(shadersDirectory + "rendering/render_flow_X.vert"));
		flowXrenderProg.addShader(GL_FRAGMENT_SHADER, readFileToString(shadersDirectory + "rendering/render_flow_X&Y.frag"));
		flowXrenderProg.compile();

		flowYrenderProg.addShader(GL_VERTEX_SHADER, readFileToString(shadersDirectory + "rendering/render_flow_Y.vert"));
		flowYrenderProg.addShader(GL_FRAGMENT_SHADER, readFileToString(shadersDirectory + "rendering/render_flow_X&Y.frag"));
		flowYrenderProg.compile();

		divergenceComputeProg.addShader(GL_COMPUTE_SHADER, readFileToString(shadersDirectory + "compute/divergence_solver.comp"));
		divergenceComputeProg.compile();

		pressureComputeProg.addShader(GL_COMPUTE_SHADER, readFileToString(shadersDirectory + "compute/pressure_solver.comp"));
		pressureComputeProg.compile();

		velocityXComputeProg.addShader(GL_COMPUTE_SHADER, readFileToString(shadersDirectory + "compute/velocity_X_solver.comp"));
		velocityXComputeProg.compile();

		velocityYComputeProg.addShader(GL_COMPUTE_SHADER, readFileToString(shadersDirectory + "compute/velocity_Y_solver.comp"));
		velocityYComputeProg.compile();

		advectionXComputeProg.addShader(GL_COMPUTE_SHADER, readFileToString(shadersDirectory + "compute/velocity_X_advection.comp"));
		advectionXComputeProg.compile();

		advectionYComputeProg.addShader(GL_COMPUTE_SHADER, readFileToString(shadersDirectory + "compute/velocity_Y_advection.comp"));
		advectionYComputeProg.compile();

		for (int i = 0; i < 2; i++) {

			divergenceData[i].setFilter(GL_NEAREST, GL_NEAREST);
			flowX[i].setFilter(GL_NEAREST, GL_NEAREST);
			flowY[i].setFilter(GL_NEAREST, GL_NEAREST);

			divergenceData[i].setWrap(GL_REPEAT, GL_REPEAT);
			flowX[i].setWrap(GL_REPEAT, GL_REPEAT);
			flowY[i].setWrap(GL_REPEAT, GL_REPEAT);

			divergenceData[i].loadImage(GL_READ_WRITE, 0, GL_FALSE, 0, GL_RG32F);
			flowX[i].loadImage(GL_READ_WRITE, 0, GL_FALSE, 0, GL_R16F);
			flowY[i].loadImage(GL_READ_WRITE, 0, GL_FALSE, 0, GL_R16F);

			divergenceData[i].loadTexture();
			flowX[i].loadTexture();
			flowY[i].loadTexture();
		}
		cellData.setFilter(GL_NEAREST, GL_NEAREST);
		cellData.setWrap(GL_REPEAT, GL_REPEAT);
		cellData.loadImage(GL_READ_WRITE, 0, GL_FALSE, 0, GL_RGBA32F);
		cellData.loadTexture();
	}

	void render_cells(int render_mode, float render_intensity) {
		cellRenderProg.useProgram();

		glUniformHandleui64ARB(glGetUniformLocation(cellRenderProg.getID(), "divergenceData_Texture"), divergenceData[current_divergence_data].getTextureHandle());
		glUniformHandleui64ARB(glGetUniformLocation(cellRenderProg.getID(), "cell_texture"), cellData.getTextureHandle());
		glUniformHandleui64ARB(glGetUniformLocation(cellRenderProg.getID(), "flowX_texture"), flowX[current_flow_data].getTextureHandle());
		glUniformHandleui64ARB(glGetUniformLocation(cellRenderProg.getID(), "flowY_texture"), flowY[current_flow_data].getTextureHandle());

		glUniform2ui(glGetUniformLocation(cellRenderProg.getID(), "cell_count"), cell_count.x, cell_count.y);
		glUniform2i(glGetUniformLocation(cellRenderProg.getID(), "resolution"), getCurrentContext()->getFormat()->width, getCurrentContext()->getFormat()->height);
		glUniform1i(glGetUniformLocation(cellRenderProg.getID(), "render_mode"), render_mode);
		glUniform1f(glGetUniformLocation(cellRenderProg.getID(), "render_intensivity"), render_intensity);

		vao.draw(GL_TRIANGLES, 6);
	}
	void render_main_velocities(float arrow_scale, float arrow_value, glm::vec4 color) {
		flowXrenderProg.useProgram();

		glUniformHandleui64ARB(glGetUniformLocation(flowXrenderProg.getID(), "flowX_texture"), flowX[current_flow_data].getTextureHandle());
		glUniform4f(glGetUniformLocation(flowXrenderProg.getID(), "vector_color"), color.r, color.g, color.b, color.a);
		glUniform2ui(glGetUniformLocation(flowXrenderProg.getID(), "cell_count"), cell_count.x, cell_count.y);
		glUniform2f(glGetUniformLocation(flowYrenderProg.getID(), "vector_scale"), arrow_scale, arrow_value);

		vao.draw(GL_TRIANGLES, 9 * (cell_count.x + 1) * (cell_count.y));

		flowYrenderProg.useProgram();

		glUniformHandleui64ARB(glGetUniformLocation(flowYrenderProg.getID(), "flowY_texture"), flowY[current_flow_data].getTextureHandle());
		glUniform4f(glGetUniformLocation(flowXrenderProg.getID(), "vector_color"), color.r, color.g, color.b, color.a);
		glUniform2ui(glGetUniformLocation(flowYrenderProg.getID(), "cell_count"), cell_count.x, cell_count.y);
		glUniform2f(glGetUniformLocation(flowYrenderProg.getID(), "vector_scale"), arrow_scale, arrow_value);

		vao.draw(GL_TRIANGLES, 9 * (cell_count.x) * (cell_count.y + 1));
	}

	void compute_divergence(float dt, float density) {
		divergenceComputeProg.useProgram();

		glUniformHandleui64ARB(glGetUniformLocation(divergenceComputeProg.getID(), "divergenceData_Texture_in"), divergenceData[current_divergence_data].getImageHandle());
		glUniformHandleui64ARB(glGetUniformLocation(divergenceComputeProg.getID(), "flowX_Texture_in"), flowX[current_flow_data].getImageHandle());
		glUniformHandleui64ARB(glGetUniformLocation(divergenceComputeProg.getID(), "flowY_Texture_in"), flowY[current_flow_data].getImageHandle());

		glUniformHandleui64ARB(glGetUniformLocation(divergenceComputeProg.getID(), "cellData_Texture"), cellData.getImageHandle());

		glUniformHandleui64ARB(glGetUniformLocation(divergenceComputeProg.getID(), "divergenceData_Texture_out"), divergenceData[!current_divergence_data].getImageHandle());

		glUniform2ui(glGetUniformLocation(divergenceComputeProg.getID(), "cell_count"), cell_count.x, cell_count.y);
		glUniform1f(glGetUniformLocation(divergenceComputeProg.getID(), "K"), dt / (density * (1.0f / (float)cell_count.x)));

		divergenceComputeProg.runCompute(cell_count.x, cell_count.y, 1, GL_ALL_BARRIER_BITS);

		current_divergence_data = !current_divergence_data;
	}
	void compute_pressure(unsigned int iterations, float SOR) {
		pressureComputeProg.useProgram();
		for (unsigned int i = 0; i < iterations; i++) {
			// red
			glUniformHandleui64ARB(glGetUniformLocation(pressureComputeProg.getID(), "divergenceData_Texture_in"), divergenceData[current_divergence_data].getImageHandle());

			glUniformHandleui64ARB(glGetUniformLocation(pressureComputeProg.getID(), "cellData_Texture"), cellData.getImageHandle());

			glUniform2ui(glGetUniformLocation(pressureComputeProg.getID(), "cell_count"), cell_count.x, cell_count.y);
			glUniform1i(glGetUniformLocation(pressureComputeProg.getID(), "red"), true);
			glUniform1f(glGetUniformLocation(pressureComputeProg.getID(), "SOR"), SOR);

			pressureComputeProg.runCompute(cell_count.x / 2, cell_count.y, 1, GL_ALL_BARRIER_BITS);

			// black
			glUniformHandleui64ARB(glGetUniformLocation(pressureComputeProg.getID(), "divergenceData_Texture_in"), divergenceData[current_divergence_data].getImageHandle());

			glUniformHandleui64ARB(glGetUniformLocation(pressureComputeProg.getID(), "cellData_Texture"), cellData.getImageHandle());

			glUniform2ui(glGetUniformLocation(pressureComputeProg.getID(), "cell_count"), cell_count.x, cell_count.y);
			glUniform1i(glGetUniformLocation(pressureComputeProg.getID(), "red"), false);
			glUniform1f(glGetUniformLocation(pressureComputeProg.getID(), "SOR"), SOR);

			pressureComputeProg.runCompute(cell_count.x / 2, cell_count.y, 1, GL_ALL_BARRIER_BITS);
		}
	}
	void compute_velocities(float dt, float density) {
		// velocity X compute
		velocityXComputeProg.useProgram();

		glUniformHandleui64ARB(glGetUniformLocation(velocityXComputeProg.getID(), "cellData_Texture"), cellData.getImageHandle());
		glUniformHandleui64ARB(glGetUniformLocation(velocityXComputeProg.getID(), "flowX_Texture_in"), flowX[current_flow_data].getImageHandle());

		glUniformHandleui64ARB(glGetUniformLocation(velocityXComputeProg.getID(), "flowX_Texture_out"), flowX[!current_flow_data].getImageHandle());

		glUniform2ui(glGetUniformLocation(velocityXComputeProg.getID(), "cell_count"), cell_count.x, cell_count.y);
		glUniform1f(glGetUniformLocation(velocityXComputeProg.getID(), "cell_side_length"), 1.0f / (float)cell_count.x);
		glUniform1f(glGetUniformLocation(velocityXComputeProg.getID(), "dt"), dt);
		glUniform1f(glGetUniformLocation(velocityXComputeProg.getID(), "density"), density);

		velocityXComputeProg.runCompute(cell_count.x + 1, cell_count.y, 1, GL_ALL_BARRIER_BITS);

		// velocity Y compute
		velocityYComputeProg.useProgram();

		glUniformHandleui64ARB(glGetUniformLocation(velocityYComputeProg.getID(), "cellData_Texture"), cellData.getImageHandle());
		glUniformHandleui64ARB(glGetUniformLocation(velocityYComputeProg.getID(), "flowY_Texture_in"), flowY[current_flow_data].getImageHandle());

		glUniformHandleui64ARB(glGetUniformLocation(velocityYComputeProg.getID(), "flowY_Texture_out"), flowY[!current_flow_data].getImageHandle());

		glUniform2ui(glGetUniformLocation(velocityYComputeProg.getID(), "cell_count"), cell_count.x, cell_count.y);
		glUniform1f(glGetUniformLocation(velocityYComputeProg.getID(), "cell_side_length"), 1.0f / (float)cell_count.x);
		glUniform1f(glGetUniformLocation(velocityYComputeProg.getID(), "dt"), dt);
		glUniform1f(glGetUniformLocation(velocityYComputeProg.getID(), "density"), density);

		velocityYComputeProg.runCompute(cell_count.x, cell_count.y + 1, 1, GL_ALL_BARRIER_BITS);

		current_flow_data = !current_flow_data;
	}
	void compute_velocity_advection(float dt) {
		// velocity X compute
		advectionXComputeProg.useProgram();

		glUniformHandleui64ARB(glGetUniformLocation(advectionXComputeProg.getID(), "flowX_Texture_in"), flowX[current_flow_data].getImageHandle());
		glUniformHandleui64ARB(glGetUniformLocation(advectionXComputeProg.getID(), "flowY_Texture_in"), flowY[current_flow_data].getImageHandle());

		glUniformHandleui64ARB(glGetUniformLocation(advectionXComputeProg.getID(), "flowX_Texture_out"), flowX[!current_flow_data].getImageHandle());

		glUniform2ui(glGetUniformLocation(advectionXComputeProg.getID(), "cell_count"), cell_count.x, cell_count.y);
		glUniform1f(glGetUniformLocation(advectionXComputeProg.getID(), "cell_side_length"), 1.0f / (float)cell_count.x);
		glUniform1f(glGetUniformLocation(advectionXComputeProg.getID(), "dt"), dt);

		advectionXComputeProg.runCompute(cell_count.x + 1, cell_count.y, 1, GL_ALL_BARRIER_BITS);

		// velocity Y compute
		advectionYComputeProg.useProgram();

		glUniformHandleui64ARB(glGetUniformLocation(advectionYComputeProg.getID(), "flowX_Texture_in"), flowX[current_flow_data].getImageHandle());
		glUniformHandleui64ARB(glGetUniformLocation(advectionYComputeProg.getID(), "flowY_Texture_in"), flowY[current_flow_data].getImageHandle());

		glUniformHandleui64ARB(glGetUniformLocation(advectionYComputeProg.getID(), "flowY_Texture_out"), flowY[!current_flow_data].getImageHandle());

		glUniform2ui(glGetUniformLocation(advectionYComputeProg.getID(), "cell_count"), cell_count.x, cell_count.y);
		glUniform1f(glGetUniformLocation(advectionYComputeProg.getID(), "cell_side_length"), 1.0f / (float)cell_count.x);
		glUniform1f(glGetUniformLocation(advectionYComputeProg.getID(), "dt"), dt);

		advectionYComputeProg.runCompute(cell_count.x, cell_count.y + 1, 1, GL_ALL_BARRIER_BITS);

		current_flow_data = !current_flow_data;
	}

	glm::ivec2 getGridSize() {
		return cell_count;
	}
};