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

class FluidGrid {
private:
	ShaderProgram cellRenderProg;
	ShaderProgram velocityXrenderProg;
	ShaderProgram velocityYrenderProg;
	ShaderProgram divergenceComputeProg;
	ShaderProgram pressureComputeProg;
	ShaderProgram velocityXComputeProg;
	ShaderProgram velocityYComputeProg;
	ShaderProgram velocityXAdvectionComputeProg;
	ShaderProgram velocityYAdvectionComputeProg;
	VAO vao;

	glm::ivec2 cell_count = { 6, 6 };

	Texture pressure_texture = {
		Texture(GL_TEXTURE_2D, 1, GL_R32F, cell_count.x, cell_count.y)
	};
	Texture divergence_texture[2] = {
		Texture(GL_TEXTURE_2D, 1, GL_RG32F, cell_count.x, cell_count.y),
		Texture(GL_TEXTURE_2D, 1, GL_RG32F, cell_count.x, cell_count.y)
	};
	Texture velocities_X_texture[2] = {
		Texture(GL_TEXTURE_2D, 1, GL_R16F, cell_count.x + 1, cell_count.y),
		Texture(GL_TEXTURE_2D, 1, GL_R16F, cell_count.x + 1, cell_count.y)
	};
	Texture velocities_Y_texture[2] = {
		Texture(GL_TEXTURE_2D, 1, GL_R16F, cell_count.x, cell_count.y + 1),
		Texture(GL_TEXTURE_2D, 1, GL_R16F, cell_count.x, cell_count.y + 1)
	};

	bool current_velocity_data = false;
	bool current_divergence_data = false;

public:
	FluidGrid(glm::vec2 cell_count)
		: cell_count(cell_count) {
		std::string shadersDirectory = "shaders/";

		cellRenderProg.addShader(GL_VERTEX_SHADER, readFileToString(shadersDirectory + "rendering/render_cell.vert"));
		cellRenderProg.addShader(GL_FRAGMENT_SHADER, readFileToString(shadersDirectory + "rendering/render_cell.frag"));
		cellRenderProg.compile();

		velocityXrenderProg.addShader(GL_VERTEX_SHADER, readFileToString(shadersDirectory + "rendering/render_flow_X.vert"));
		velocityXrenderProg.addShader(GL_FRAGMENT_SHADER, readFileToString(shadersDirectory + "rendering/render_flow_X&Y.frag"));
		velocityXrenderProg.compile();

		velocityYrenderProg.addShader(GL_VERTEX_SHADER, readFileToString(shadersDirectory + "rendering/render_flow_Y.vert"));
		velocityYrenderProg.addShader(GL_FRAGMENT_SHADER, readFileToString(shadersDirectory + "rendering/render_flow_X&Y.frag"));
		velocityYrenderProg.compile();

		divergenceComputeProg.addShader(GL_COMPUTE_SHADER, readFileToString(shadersDirectory + "compute/divergence_solver.comp"));
		divergenceComputeProg.compile();

		pressureComputeProg.addShader(GL_COMPUTE_SHADER, readFileToString(shadersDirectory + "compute/pressure_solver.comp"));
		pressureComputeProg.compile();

		velocityXComputeProg.addShader(GL_COMPUTE_SHADER, readFileToString(shadersDirectory + "compute/velocity_X_solver.comp"));
		velocityXComputeProg.compile();

		velocityYComputeProg.addShader(GL_COMPUTE_SHADER, readFileToString(shadersDirectory + "compute/velocity_Y_solver.comp"));
		velocityYComputeProg.compile();

		velocityXAdvectionComputeProg.addShader(GL_COMPUTE_SHADER, readFileToString(shadersDirectory + "compute/velocity_X_advection.comp"));
		velocityXAdvectionComputeProg.compile();

		velocityYAdvectionComputeProg.addShader(GL_COMPUTE_SHADER, readFileToString(shadersDirectory + "compute/velocity_Y_advection.comp"));
		velocityYAdvectionComputeProg.compile();

		for (int i = 0; i < 2; i++) {

			divergence_texture[i].setFilter(GL_NEAREST, GL_NEAREST);
			velocities_X_texture[i].setFilter(GL_NEAREST, GL_NEAREST);
			velocities_Y_texture[i].setFilter(GL_NEAREST, GL_NEAREST);

			divergence_texture[i].setWrap(GL_REPEAT, GL_REPEAT);
			velocities_X_texture[i].setWrap(GL_REPEAT, GL_REPEAT);
			velocities_Y_texture[i].setWrap(GL_REPEAT, GL_REPEAT);

			divergence_texture[i].loadImage(GL_READ_WRITE, 0, GL_FALSE, 0, GL_RG32F);
			velocities_X_texture[i].loadImage(GL_READ_WRITE, 0, GL_FALSE, 0, GL_R16F);
			velocities_Y_texture[i].loadImage(GL_READ_WRITE, 0, GL_FALSE, 0, GL_R16F);

			divergence_texture[i].loadTexture();
			velocities_X_texture[i].loadTexture();
			velocities_Y_texture[i].loadTexture();
		}
		pressure_texture.setFilter(GL_NEAREST, GL_NEAREST);
		pressure_texture.setWrap(GL_REPEAT, GL_REPEAT);
		pressure_texture.loadImage(GL_READ_WRITE, 0, GL_FALSE, 0, GL_R32F);
		pressure_texture.loadTexture();
	}

	void render_cells(int render_mode, float render_intensity) {
		cellRenderProg.useProgram();

		glUniformHandleui64ARB(glGetUniformLocation(cellRenderProg.getID(), "divergence_Texture"), divergence_texture[current_divergence_data].getTextureHandle());
		glUniformHandleui64ARB(glGetUniformLocation(cellRenderProg.getID(), "pressure_Texture"), pressure_texture.getTextureHandle());
		glUniformHandleui64ARB(glGetUniformLocation(cellRenderProg.getID(), "flowX_Texture"), velocities_X_texture[current_velocity_data].getTextureHandle());
		glUniformHandleui64ARB(glGetUniformLocation(cellRenderProg.getID(), "flowY_Texture"), velocities_Y_texture[current_velocity_data].getTextureHandle());

		glUniform1i(glGetUniformLocation(cellRenderProg.getID(), "render_mode"), render_mode);
		glUniform1f(glGetUniformLocation(cellRenderProg.getID(), "render_intensivity"), render_intensity);

		vao.draw(GL_TRIANGLES, 6);
	}
	void render_main_velocities(float arrow_scale, float arrow_value, glm::vec4 color) {
		velocityXrenderProg.useProgram();

		glUniformHandleui64ARB(glGetUniformLocation(velocityXrenderProg.getID(), "flowX_Texture"), velocities_X_texture[current_velocity_data].getImageHandle());
		glUniform4f(glGetUniformLocation(velocityXrenderProg.getID(), "vector_color"), color.r, color.g, color.b, color.a);
		glUniform2i(glGetUniformLocation(velocityXrenderProg.getID(), "cell_count"), cell_count.x, cell_count.y);
		glUniform2f(glGetUniformLocation(velocityXrenderProg.getID(), "vector_scale"), arrow_scale, arrow_value);

		vao.draw(GL_TRIANGLES, 9 * (cell_count.x + 1) * (cell_count.y));

		velocityYrenderProg.useProgram();

		glUniformHandleui64ARB(glGetUniformLocation(velocityYrenderProg.getID(), "flowY_Texture"), velocities_Y_texture[current_velocity_data].getImageHandle());
		glUniform4f(glGetUniformLocation(velocityYrenderProg.getID(), "vector_color"), color.r, color.g, color.b, color.a);
		glUniform2i(glGetUniformLocation(velocityYrenderProg.getID(), "cell_count"), cell_count.x, cell_count.y);
		glUniform2f(glGetUniformLocation(velocityYrenderProg.getID(), "vector_scale"), arrow_scale, arrow_value);

		vao.draw(GL_TRIANGLES, 9 * (cell_count.x) * (cell_count.y + 1));
	}

	void compute_divergence(float dt, float density) {
		divergenceComputeProg.useProgram();

		glUniformHandleui64ARB(glGetUniformLocation(divergenceComputeProg.getID(), "divergence_Texture_in"), divergence_texture[current_divergence_data].getImageHandle());
		glUniformHandleui64ARB(glGetUniformLocation(divergenceComputeProg.getID(), "flowX_Texture_in"), velocities_X_texture[current_velocity_data].getImageHandle());
		glUniformHandleui64ARB(glGetUniformLocation(divergenceComputeProg.getID(), "flowY_Texture_in"), velocities_Y_texture[current_velocity_data].getImageHandle());

		glUniformHandleui64ARB(glGetUniformLocation(divergenceComputeProg.getID(), "pressure_Texture"), pressure_texture.getImageHandle());

		glUniformHandleui64ARB(glGetUniformLocation(divergenceComputeProg.getID(), "divergence_Texture_out"), divergence_texture[!current_divergence_data].getImageHandle());

		glUniform2i(glGetUniformLocation(divergenceComputeProg.getID(), "cell_count"), cell_count.x, cell_count.y);
		glUniform1f(glGetUniformLocation(divergenceComputeProg.getID(), "K"), dt / (density * (1.0f / (float)cell_count.x)));

		divergenceComputeProg.runCompute(cell_count.x, cell_count.y, 1, GL_ALL_BARRIER_BITS);

		current_divergence_data = !current_divergence_data;
	}
	void compute_pressure(unsigned int iterations, float SOR) {
		pressureComputeProg.useProgram();
		for (unsigned int i = 0; i < iterations; i++) {
			// red
			glUniformHandleui64ARB(glGetUniformLocation(pressureComputeProg.getID(), "divergence_Texture_in"), divergence_texture[current_divergence_data].getImageHandle());

			glUniformHandleui64ARB(glGetUniformLocation(pressureComputeProg.getID(), "pressure_Texture"), pressure_texture.getImageHandle());

			glUniform2i(glGetUniformLocation(pressureComputeProg.getID(), "cell_count"), cell_count.x, cell_count.y);
			glUniform1i(glGetUniformLocation(pressureComputeProg.getID(), "red"), true);
			glUniform1f(glGetUniformLocation(pressureComputeProg.getID(), "SOR"), SOR);

			pressureComputeProg.runCompute(cell_count.x / 2, cell_count.y, 1, GL_ALL_BARRIER_BITS);

			// black
			glUniformHandleui64ARB(glGetUniformLocation(pressureComputeProg.getID(), "divergence_Texture_in"), divergence_texture[current_divergence_data].getImageHandle());

			glUniformHandleui64ARB(glGetUniformLocation(pressureComputeProg.getID(), "pressure_Texture"), pressure_texture.getImageHandle());

			glUniform2i(glGetUniformLocation(pressureComputeProg.getID(), "cell_count"), cell_count.x, cell_count.y);
			glUniform1i(glGetUniformLocation(pressureComputeProg.getID(), "red"), false);
			glUniform1f(glGetUniformLocation(pressureComputeProg.getID(), "SOR"), SOR);

			pressureComputeProg.runCompute(cell_count.x / 2, cell_count.y, 1, GL_ALL_BARRIER_BITS);
		}
	}
	void compute_velocities(float dt, float density) {
		// velocity X compute
		velocityXComputeProg.useProgram();

		glUniformHandleui64ARB(glGetUniformLocation(velocityXComputeProg.getID(), "pressure_Texture"), pressure_texture.getImageHandle());
		glUniformHandleui64ARB(glGetUniformLocation(velocityXComputeProg.getID(), "flowX_Texture_in"), velocities_X_texture[current_velocity_data].getImageHandle());

		glUniformHandleui64ARB(glGetUniformLocation(velocityXComputeProg.getID(), "flowX_Texture_out"), velocities_X_texture[!current_velocity_data].getImageHandle());

		glUniform2i(glGetUniformLocation(velocityXComputeProg.getID(), "cell_count"), cell_count.x, cell_count.y);
		glUniform1f(glGetUniformLocation(velocityXComputeProg.getID(), "cell_side_length"), 1.0f / (float)cell_count.x);
		glUniform1f(glGetUniformLocation(velocityXComputeProg.getID(), "dt"), dt);
		glUniform1f(glGetUniformLocation(velocityXComputeProg.getID(), "density"), density);

		velocityXComputeProg.runCompute(cell_count.x + 1, cell_count.y, 1, GL_ALL_BARRIER_BITS);

		// velocity Y compute
		velocityYComputeProg.useProgram();

		glUniformHandleui64ARB(glGetUniformLocation(velocityYComputeProg.getID(), "pressure_Texture"), pressure_texture.getImageHandle());
		glUniformHandleui64ARB(glGetUniformLocation(velocityYComputeProg.getID(), "flowY_Texture_in"), velocities_Y_texture[current_velocity_data].getImageHandle());

		glUniformHandleui64ARB(glGetUniformLocation(velocityYComputeProg.getID(), "flowY_Texture_out"), velocities_Y_texture[!current_velocity_data].getImageHandle());

		glUniform2i(glGetUniformLocation(velocityYComputeProg.getID(), "cell_count"), cell_count.x, cell_count.y);
		glUniform1f(glGetUniformLocation(velocityYComputeProg.getID(), "cell_side_length"), 1.0f / (float)cell_count.x);
		glUniform1f(glGetUniformLocation(velocityYComputeProg.getID(), "dt"), dt);
		glUniform1f(glGetUniformLocation(velocityYComputeProg.getID(), "density"), density);

		velocityYComputeProg.runCompute(cell_count.x, cell_count.y + 1, 1, GL_ALL_BARRIER_BITS);

		current_velocity_data = !current_velocity_data;
	}
	void compute_velocity_advection(float dt) {
		// velocity X compute
		velocityXAdvectionComputeProg.useProgram();

		glUniformHandleui64ARB(glGetUniformLocation(velocityXAdvectionComputeProg.getID(), "flowX_Texture_in"), velocities_X_texture[current_velocity_data].getImageHandle());
		glUniformHandleui64ARB(glGetUniformLocation(velocityXAdvectionComputeProg.getID(), "flowY_Texture_in"), velocities_Y_texture[current_velocity_data].getImageHandle());

		glUniformHandleui64ARB(glGetUniformLocation(velocityXAdvectionComputeProg.getID(), "flowX_Texture_out"), velocities_X_texture[!current_velocity_data].getImageHandle());

		glUniform2i(glGetUniformLocation(velocityXAdvectionComputeProg.getID(), "cell_count"), cell_count.x, cell_count.y);
		glUniform1f(glGetUniformLocation(velocityXAdvectionComputeProg.getID(), "cell_side_length"), 1.0f / (float)cell_count.x);
		glUniform1f(glGetUniformLocation(velocityXAdvectionComputeProg.getID(), "dt"), dt);

		velocityXAdvectionComputeProg.runCompute(cell_count.x + 1, cell_count.y, 1, GL_ALL_BARRIER_BITS);

		// velocity Y compute
		velocityYAdvectionComputeProg.useProgram();

		glUniformHandleui64ARB(glGetUniformLocation(velocityYAdvectionComputeProg.getID(), "flowX_Texture_in"), velocities_X_texture[current_velocity_data].getImageHandle());
		glUniformHandleui64ARB(glGetUniformLocation(velocityYAdvectionComputeProg.getID(), "flowY_Texture_in"), velocities_Y_texture[current_velocity_data].getImageHandle());

		glUniformHandleui64ARB(glGetUniformLocation(velocityYAdvectionComputeProg.getID(), "flowY_Texture_out"), velocities_Y_texture[!current_velocity_data].getImageHandle());

		glUniform2i(glGetUniformLocation(velocityYAdvectionComputeProg.getID(), "cell_count"), cell_count.x, cell_count.y);
		glUniform1f(glGetUniformLocation(velocityYAdvectionComputeProg.getID(), "cell_side_length"), 1.0f / (float)cell_count.x);
		glUniform1f(glGetUniformLocation(velocityYAdvectionComputeProg.getID(), "dt"), dt);

		velocityYAdvectionComputeProg.runCompute(cell_count.x, cell_count.y + 1, 1, GL_ALL_BARRIER_BITS);

		current_velocity_data = !current_velocity_data;
	}

	Texture* pressure_tex() {
		return &pressure_texture;
	}
	Texture* divergence_tex() {
		return &divergence_texture[current_divergence_data];
	}
	Texture* velocity_X_tex() {
		return &velocities_X_texture[current_velocity_data];
	}
	Texture* velocity_Y_tex() {
		return &velocities_Y_texture[current_velocity_data];
	}

	glm::ivec2 getGridSize() {
		return cell_count;
	}
};