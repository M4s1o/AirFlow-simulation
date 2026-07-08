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
	ShaderProgram obstacleRenderProg;
	ShaderProgram velocityXrenderProg;
	ShaderProgram velocityYrenderProg;

	ShaderProgram divergenceComputeProg;
	ShaderProgram pressureComputeProg;
	ShaderProgram velocityXComputeProg;
	ShaderProgram velocityYComputeProg;
	ShaderProgram velocityXAdvectionComputeProg;
	ShaderProgram velocityYAdvectionComputeProg;
	ShaderProgram attributeAdvectionComputeProg;

	ShaderProgram circleDrawComputeProg;

	VAO vao;

	glm::ivec2 cell_count = { 0, 0 };

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
	Texture attribute_texture[2] = {
		Texture(GL_TEXTURE_2D, 1, GL_RGBA16F, cell_count.x, cell_count.y),
		Texture(GL_TEXTURE_2D, 1, GL_RGBA16F, cell_count.x, cell_count.y)
	};
	Texture obstacle_texture = {
		Texture(GL_TEXTURE_2D, 1, GL_R8UI, cell_count.x, cell_count.y)
	};

	enum TextureUnit : GLuint {
		TEXTURE_UNIT_0 = 0,
		TEXTURE_UNIT_1 = 1,
		TEXTURE_UNIT_2 = 2,
		TEXTURE_UNIT_3 = 3,
		TEXTURE_UNIT_4 = 4
	};

	enum ImageUnit : GLuint {
		IMAGE_UNIT_0 = 0,
		IMAGE_UNIT_1 = 1,
		IMAGE_UNIT_2 = 2,
		IMAGE_UNIT_3 = 3,
		IMAGE_UNIT_4 = 4,
		IMAGE_UNIT_5 = 5
	};

	bool current_velocity_data = false;
	bool current_divergence_data = false;
	bool current_attribute_data = false;

public:
	FluidGrid(glm::vec2 cell_count)
		: cell_count(cell_count) {
		std::string shadersDirectory = "shaders/";

		cellRenderProg.addShader(GL_VERTEX_SHADER, readFileToString(shadersDirectory + "rendering/render_cell.vert"));
		cellRenderProg.addShader(GL_FRAGMENT_SHADER, readFileToString(shadersDirectory + "rendering/render_cell.frag"));
		cellRenderProg.compile();

		obstacleRenderProg.addShader(GL_VERTEX_SHADER, readFileToString(shadersDirectory + "rendering/render_cell.vert"));
		obstacleRenderProg.addShader(GL_FRAGMENT_SHADER, readFileToString(shadersDirectory + "rendering/render_obstacles.frag"));
		obstacleRenderProg.compile();

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

		attributeAdvectionComputeProg.addShader(GL_COMPUTE_SHADER, readFileToString(shadersDirectory + "compute/attribute_advection.comp"));
		attributeAdvectionComputeProg.compile();

		circleDrawComputeProg.addShader(GL_COMPUTE_SHADER, readFileToString(shadersDirectory + "modify/circle.comp"));
		circleDrawComputeProg.compile();

		for (int i = 0; i < 2; i++) {

			divergence_texture[i].setFilter(GL_NEAREST, GL_NEAREST);
			velocities_X_texture[i].setFilter(GL_LINEAR, GL_LINEAR);
			velocities_Y_texture[i].setFilter(GL_LINEAR, GL_LINEAR);
			attribute_texture[i].setFilter(GL_NEAREST, GL_NEAREST);

			divergence_texture[i].setWrap(GL_REPEAT, GL_REPEAT);
			velocities_X_texture[i].setWrap(GL_REPEAT, GL_REPEAT);
			velocities_Y_texture[i].setWrap(GL_REPEAT, GL_REPEAT);
			attribute_texture[i].setWrap(GL_REPEAT, GL_REPEAT);

			divergence_texture[i].loadImage(GL_READ_WRITE, 0, GL_FALSE, 0, GL_RG32F);
			velocities_X_texture[i].loadImage(GL_READ_WRITE, 0, GL_FALSE, 0, GL_R16F);
			velocities_Y_texture[i].loadImage(GL_READ_WRITE, 0, GL_FALSE, 0, GL_R16F);
			attribute_texture[i].loadImage(GL_READ_WRITE, 0, GL_FALSE, 0, GL_RGBA16F);
		}
		pressure_texture.setFilter(GL_NEAREST, GL_NEAREST);
		pressure_texture.setWrap(GL_REPEAT, GL_REPEAT);
		pressure_texture.loadImage(GL_READ_WRITE, 0, GL_FALSE, 0, GL_R32F);

		obstacle_texture.setFilter(GL_NEAREST, GL_NEAREST);
		obstacle_texture.setWrap(GL_REPEAT, GL_REPEAT);
		obstacle_texture.loadImage(GL_READ_WRITE, 0, GL_FALSE, 0, GL_R8UI);

		reset();
	}

	void render_cells(int render_mode, float render_intensity) {
		cellRenderProg.useProgram();

		divergence_texture[current_divergence_data].bindTexture(TEXTURE_UNIT_0);
		pressure_texture.bindTexture(TEXTURE_UNIT_1);
		velocities_X_texture[current_velocity_data].bindTexture(TEXTURE_UNIT_2);
		velocities_Y_texture[current_velocity_data].bindTexture(TEXTURE_UNIT_3);
		attribute_texture[current_attribute_data].bindTexture(TEXTURE_UNIT_4);

		glUniform1i(glGetUniformLocation(cellRenderProg.getID(), "render_mode"), render_mode);
		glUniform1f(glGetUniformLocation(cellRenderProg.getID(), "render_intensivity"), render_intensity);

		vao.draw(GL_TRIANGLES, 6);
	}
	void render_obstacles(glm::vec4 color) {
		obstacleRenderProg.useProgram();

		obstacle_texture.bindTexture(TEXTURE_UNIT_0);

		glUniform4f(glGetUniformLocation(obstacleRenderProg.getID(), "obstacle_color"), color.r, color.g, color.b, color.a);

		vao.draw(GL_TRIANGLES, 6);
	}
	void render_main_velocities(float arrow_scale, float arrow_value, glm::vec4 color) {
		velocityXrenderProg.useProgram();

		velocities_X_texture[current_velocity_data].bindTexture(TEXTURE_UNIT_0);
		glUniform4f(glGetUniformLocation(velocityXrenderProg.getID(), "vector_color"), color.r, color.g, color.b, color.a);
		glUniform2i(glGetUniformLocation(velocityXrenderProg.getID(), "cell_count"), cell_count.x, cell_count.y);
		glUniform2f(glGetUniformLocation(velocityXrenderProg.getID(), "vector_scale"), arrow_scale, arrow_value);

		vao.draw(GL_TRIANGLES, 9 * (cell_count.x + 1) * (cell_count.y));

		velocityYrenderProg.useProgram();

		velocities_Y_texture[current_velocity_data].bindTexture(TEXTURE_UNIT_0);
		glUniform4f(glGetUniformLocation(velocityYrenderProg.getID(), "vector_color"), color.r, color.g, color.b, color.a);
		glUniform2i(glGetUniformLocation(velocityYrenderProg.getID(), "cell_count"), cell_count.x, cell_count.y);
		glUniform2f(glGetUniformLocation(velocityYrenderProg.getID(), "vector_scale"), arrow_scale, arrow_value);

		vao.draw(GL_TRIANGLES, 9 * (cell_count.x) * (cell_count.y + 1));
	}

	void compute_divergence(float dt, float density) {
		divergenceComputeProg.useProgram();

		divergence_texture[current_divergence_data].bindImage(IMAGE_UNIT_0);
		velocities_X_texture[current_velocity_data].bindImage(IMAGE_UNIT_1);
		velocities_Y_texture[current_velocity_data].bindImage(IMAGE_UNIT_2);
		pressure_texture.bindImage(IMAGE_UNIT_3);
		obstacle_texture.bindImage(IMAGE_UNIT_4);
		divergence_texture[!current_divergence_data].bindImage(IMAGE_UNIT_5);

		glUniform2i(glGetUniformLocation(divergenceComputeProg.getID(), "cell_count"), cell_count.x, cell_count.y);
		glUniform1f(glGetUniformLocation(divergenceComputeProg.getID(), "K"), dt / (density * (1.0f / (float)cell_count.x)));

		divergenceComputeProg.runCompute(cell_count.x, cell_count.y, 1, GL_ALL_BARRIER_BITS);

		current_divergence_data = !current_divergence_data;
	}
	void compute_pressure(unsigned int iterations, float SOR) {
		pressureComputeProg.useProgram();

		divergence_texture[current_divergence_data].bindImage(IMAGE_UNIT_0);
		obstacle_texture.bindImage(IMAGE_UNIT_1);
		pressure_texture.bindImage(IMAGE_UNIT_2);

		for (unsigned int i = 0; i < iterations; i++) {
			// red
			glUniform2i(glGetUniformLocation(pressureComputeProg.getID(), "cell_count"), cell_count.x, cell_count.y);
			glUniform1i(glGetUniformLocation(pressureComputeProg.getID(), "red"), true);
			glUniform1f(glGetUniformLocation(pressureComputeProg.getID(), "SOR"), SOR);

			pressureComputeProg.runCompute(cell_count.x / 2, cell_count.y, 1, GL_ALL_BARRIER_BITS);

			// black
			glUniform2i(glGetUniformLocation(pressureComputeProg.getID(), "cell_count"), cell_count.x, cell_count.y);
			glUniform1i(glGetUniformLocation(pressureComputeProg.getID(), "red"), false);
			glUniform1f(glGetUniformLocation(pressureComputeProg.getID(), "SOR"), SOR);

			pressureComputeProg.runCompute(cell_count.x / 2, cell_count.y, 1, GL_ALL_BARRIER_BITS);
		}
	}
	void compute_velocities(float dt, float density) {
		// velocity X compute
		velocityXComputeProg.useProgram();

		pressure_texture.bindImage(IMAGE_UNIT_0);
		velocities_X_texture[current_velocity_data].bindImage(IMAGE_UNIT_1);
		obstacle_texture.bindImage(IMAGE_UNIT_2);
		velocities_X_texture[!current_velocity_data].bindImage(IMAGE_UNIT_3);

		glUniform2i(glGetUniformLocation(velocityXComputeProg.getID(), "cell_count"), cell_count.x, cell_count.y);
		glUniform1f(glGetUniformLocation(velocityXComputeProg.getID(), "cell_side_length"), 1.0f / (float)cell_count.x);
		glUniform1f(glGetUniformLocation(velocityXComputeProg.getID(), "dt"), dt);
		glUniform1f(glGetUniformLocation(velocityXComputeProg.getID(), "density"), density);

		velocityXComputeProg.runCompute(cell_count.x + 1, cell_count.y, 1, GL_ALL_BARRIER_BITS);

		// velocity Y compute
		velocityYComputeProg.useProgram();

		pressure_texture.bindImage(IMAGE_UNIT_0);
		velocities_Y_texture[current_velocity_data].bindImage(IMAGE_UNIT_1);
		obstacle_texture.bindImage(IMAGE_UNIT_2);
		velocities_Y_texture[!current_velocity_data].bindImage(IMAGE_UNIT_3);

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

		velocities_X_texture[current_velocity_data].bindImage(IMAGE_UNIT_0);
		velocities_Y_texture[current_velocity_data].bindImage(IMAGE_UNIT_1);
		velocities_X_texture[!current_velocity_data].bindImage(IMAGE_UNIT_2);

		glUniform2i(glGetUniformLocation(velocityXAdvectionComputeProg.getID(), "cell_count"), cell_count.x, cell_count.y);
		glUniform1f(glGetUniformLocation(velocityXAdvectionComputeProg.getID(), "cell_side_length"), 1.0f / (float)cell_count.x);
		glUniform1f(glGetUniformLocation(velocityXAdvectionComputeProg.getID(), "dt"), dt);

		velocityXAdvectionComputeProg.runCompute(cell_count.x + 1, cell_count.y, 1, GL_ALL_BARRIER_BITS);

		// velocity Y compute
		velocityYAdvectionComputeProg.useProgram();

		velocities_X_texture[current_velocity_data].bindImage(IMAGE_UNIT_0);
		velocities_Y_texture[current_velocity_data].bindImage(IMAGE_UNIT_1);
		velocities_Y_texture[!current_velocity_data].bindImage(IMAGE_UNIT_2);

		glUniform2i(glGetUniformLocation(velocityYAdvectionComputeProg.getID(), "cell_count"), cell_count.x, cell_count.y);
		glUniform1f(glGetUniformLocation(velocityYAdvectionComputeProg.getID(), "cell_side_length"), 1.0f / (float)cell_count.x);
		glUniform1f(glGetUniformLocation(velocityYAdvectionComputeProg.getID(), "dt"), dt);

		velocityYAdvectionComputeProg.runCompute(cell_count.x, cell_count.y + 1, 1, GL_ALL_BARRIER_BITS);

		current_velocity_data = !current_velocity_data;
	}
	void compute_attribute_advection(float dt) {
		attributeAdvectionComputeProg.useProgram();

		velocities_X_texture[current_velocity_data].bindImage(IMAGE_UNIT_0);
		velocities_Y_texture[current_velocity_data].bindImage(IMAGE_UNIT_1);
		attribute_texture[current_attribute_data].bindImage(IMAGE_UNIT_2);
		obstacle_texture.bindImage(IMAGE_UNIT_3);

		attribute_texture[!current_attribute_data].bindImage(IMAGE_UNIT_4);

		glUniform2i(glGetUniformLocation(attributeAdvectionComputeProg.getID(), "cell_count"), cell_count.x, cell_count.y);
		glUniform1f(glGetUniformLocation(attributeAdvectionComputeProg.getID(), "cell_side_length"), 1.0f / (float)cell_count.x);
		glUniform1f(glGetUniformLocation(attributeAdvectionComputeProg.getID(), "dt"), dt);

		attributeAdvectionComputeProg.runCompute(cell_count.x, cell_count.y, 1, GL_ALL_BARRIER_BITS);

		current_attribute_data = !current_attribute_data;
	}

	void draw_circle(glm::vec2 center, float radius, int state) {
		circleDrawComputeProg.useProgram();

		obstacle_texture.bindImage(IMAGE_UNIT_0);

		glUniform2f(glGetUniformLocation(circleDrawComputeProg.getID(), "circle_center"), center.x, center.y);
		glUniform1f(glGetUniformLocation(circleDrawComputeProg.getID(), "radius"), radius);
		glUniform1i(glGetUniformLocation(circleDrawComputeProg.getID(), "state"), state);
		glUniform2i(glGetUniformLocation(circleDrawComputeProg.getID(), "cell_count"), cell_count.x, cell_count.y);
		glUniform1f(glGetUniformLocation(circleDrawComputeProg.getID(), "cell_side_length"), 1.0f / (float)cell_count.y);

		circleDrawComputeProg.runCompute(cell_count.x, cell_count.y, 1, GL_ALL_BARRIER_BITS);
	}

	void reset() {
		int x = cell_count.x;
		int y = cell_count.y;

		std::vector<int> zeroedObstacle(x * y, 0);
		std::vector<float> zeroedFlowX((x + 1) * y, 0.0f);
		std::vector<float> zeroedFlowY(x * (y + 1), 0.0f);
		std::vector<float> zeroedPressure(x * y, 0.0f);
		std::vector<float> zeroedDivergence(x * y * 2, 0.0f);
		std::vector<float> zeroedAttributes(x * y * 4, 0.0f);

		for (int i = 0; i < 2; i++) {
			velocities_X_texture[i].write(
				0, 0, 0,
				x + 1, y,
				GL_RED,
				GL_FLOAT,
				zeroedFlowX.data()
			);
			velocities_Y_texture[i].write(
				0, 0, 0,
				x, y + 1,
				GL_RED,
				GL_FLOAT,
				zeroedFlowY.data()
			);
			divergence_texture[i].write(
				0, 0, 0,
				x, y,
				GL_RG,
				GL_FLOAT,
				zeroedDivergence.data()
			);
			attribute_texture[i].write(
				0, 0, 0,
				x, y,
				GL_RGBA,
				GL_FLOAT,
				zeroedAttributes.data()
			);
		}

		obstacle_texture.write(
			0, 0, 0,
			x, y,
			GL_RED_INTEGER,
			GL_UNSIGNED_BYTE,
			zeroedObstacle.data()
		);
		pressure_texture.write(
			0, 0, 0,
			x, y,
			GL_RED,
			GL_FLOAT,
			zeroedPressure.data()
		);
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
	Texture* attribute_tex() {
		return &attribute_texture[current_attribute_data];
	}
	Texture* obstacle_tex() {
		return &obstacle_texture;
	}

	Texture* divergence_tex_2() {
		return &divergence_texture[!current_divergence_data];
	}
	Texture* velocity_X_tex_2() {
		return &velocities_X_texture[!current_velocity_data];
	}
	Texture* velocity_Y_tex_2() {
		return &velocities_Y_texture[!current_velocity_data];
	}
	Texture* attribute_tex_2() {
		return &attribute_texture[!current_attribute_data];
	}

	glm::ivec2 getGridSize() {
		return cell_count;
	}
};
