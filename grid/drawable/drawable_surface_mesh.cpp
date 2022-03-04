#include "drawable_surface_mesh.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <memory>

void DrawableMesh::initialize(QOpenGLContext *_context, ShaderCompiler::GLFunctions *functions) {
	this->gl			= functions;
	this->bound_context = _context;

	// Create the right shader program :
	auto compiler = std::make_unique<ShaderCompiler>(functions);
	compiler->vertexShader_file("../shaders/base_mesh.vert").fragmentShader_file("../shaders/base_mesh.frag");
	if (compiler->compileShaders()) {
		this->program_handle_draw = compiler->programName();
	} else {
		std::cerr << "Error while building shaders for drawable mesh.\n" << compiler->errorString() << "\n";
	}

	compiler->vertexShader_file("../shaders/sphere.vert").fragmentShader_file("../shaders/sphere.frag");
	if (compiler->compileShaders()) {
		this->program_handle_manipulator_draw = compiler->programName();
	} else {
		std::cerr << "Error while building manipulator shaders for drawable mesh.\n" << compiler->errorString() << "\n";
	}

    this->color = glm::vec4(1., 0., 0., 1.);
    this->lightPosition = glm::vec3(500., 500., 500.);
	this->makeVAO();
}

void DrawableMesh::draw(GLfloat *proj_mat, GLfloat *view_mat, glm::vec4 camera) {
	if (this->should_update_on_next_draw) {
		this->updateData();
	}

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Bind the VAO & program
	this->gl->glUseProgram(this->program_handle_draw);
	this->gl->glBindVertexArray(this->vao);
	this->gl->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->vbo_indices);

	GLint location_proj		    = this->gl->glGetUniformLocation(this->program_handle_draw, "proj");
	GLint location_view		    = this->gl->glGetUniformLocation(this->program_handle_draw, "view");
	GLint location_model	    = this->gl->glGetUniformLocation(this->program_handle_draw, "model");
	GLint location_camera_pos   = this->gl->glGetUniformLocation(this->program_handle_draw, "camera_pos");
	GLint location_color        = this->gl->glGetUniformLocation(this->program_handle_draw, "objectColor");
	GLint location_light        = this->gl->glGetUniformLocation(this->program_handle_draw, "lightPosition");

	this->gl->glUniformMatrix4fv(location_proj, 1, GL_FALSE, proj_mat);
	this->gl->glUniformMatrix4fv(location_view, 1, GL_FALSE, view_mat);
	//this->gl->glUniformMatrix4fv(location_model, 1, GL_FALSE, glm::value_ptr(this->transformation_matrix));
	this->gl->glUniformMatrix4fv(location_model, 1, GL_FALSE, glm::value_ptr(this->mesh->getModelMatrix()));
	this->gl->glUniform4fv(location_camera_pos, 1, glm::value_ptr(camera));
	this->gl->glUniform4fv(location_color, 1, glm::value_ptr(this->color));
	this->gl->glUniform3fv(location_light, 1, glm::value_ptr(this->lightPosition));

	// Launch a glDrawElements() command

    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    glFrontFace(GL_CW);
    glEnable(GL_FLAT);
    glShadeModel(GL_FLAT);
	this->gl->glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(this->mesh->getTriangles().size() * 3), GL_UNSIGNED_INT, 0);
    glEnable(GL_SMOOTH);
    glShadeModel(GL_SMOOTH);

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	this->gl->glUniform4fv(location_color, 1, glm::value_ptr(glm::vec4(0.5, 0.5, 0.5, 0.5)));
	this->gl->glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(this->mesh->getTriangles().size() * 3), GL_UNSIGNED_INT, 0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_CULL_FACE);

	// Unbind the VAO & program
	this->gl->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	this->gl->glBindVertexArray(0);
	this->gl->glUseProgram(0);

    //this->mesh->drawNormals();
}

void DrawableMesh::fastDraw(GLfloat *proj_mat, GLfloat *view_mat, glm::vec4 camera) {
	this->draw(proj_mat, view_mat, camera);
}

void DrawableMesh::makeVAO(void) {
	// Fetch the mesh information :
	auto vertices = this->mesh->getVertices();
	auto normals  = this->mesh->getVertexNormals();
	std::vector<glm::vec2> texture_dummy(vertices.size(), glm::vec2{});
	std::vector<GLuint> final_order;	// indices to draw the mesh with
	for (const auto &triangle : this->mesh->getTriangles()) {
		final_order.emplace_back(triangle.getVertex(0));
		final_order.emplace_back(triangle.getVertex(1));
		final_order.emplace_back(triangle.getVertex(2));
	}

	// Create the VAO :
	this->gl->glGenVertexArrays(1, &this->vao);

	// Create the VBOs :

	// vertex buffer :
	this->gl->glGenBuffers(1, &this->vbo_vertices);
	this->gl->glBindBuffer(GL_ARRAY_BUFFER, this->vbo_vertices);
	this->gl->glBufferData(GL_ARRAY_BUFFER, vertices.size() * 3 * sizeof(GLfloat), vertices.data(), GL_STATIC_DRAW);
	// normal buffer :
	this->gl->glGenBuffers(1, &this->vbo_normals);
	this->gl->glBindBuffer(GL_ARRAY_BUFFER, this->vbo_normals);
	this->gl->glBufferData(GL_ARRAY_BUFFER, normals.size() * 3 * sizeof(GLfloat), normals.data(), GL_STATIC_DRAW);
	// texture buffer :
	this->gl->glGenBuffers(1, &this->vbo_texture);
	this->gl->glBindBuffer(GL_ARRAY_BUFFER, this->vbo_texture);
	this->gl->glBufferData(GL_ARRAY_BUFFER, texture_dummy.size() * 2 * sizeof(GLfloat), texture_dummy.data(), GL_STATIC_DRAW);

	// index buffer :
	this->gl->glGenBuffers(1, &this->vbo_indices);
	this->gl->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->vbo_indices);
	this->gl->glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(final_order.size()) * sizeof(GLuint), final_order.data(), GL_STATIC_DRAW);

	// Populate the VAO :
	this->gl->glBindVertexArray(this->vao);
	// layout(location=0) : vertices (vec3)
	this->gl->glEnableVertexAttribArray(0);
	this->gl->glBindBuffer(GL_ARRAY_BUFFER, this->vbo_vertices);
	this->gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *) 0);
	// layout(location=1) : normals (vec3)
	this->gl->glEnableVertexAttribArray(1);
	this->gl->glBindBuffer(GL_ARRAY_BUFFER, this->vbo_normals);
	this->gl->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (void *) 0);
	// layout(location=2) : textures (vec2)
	this->gl->glEnableVertexAttribArray(2);
	this->gl->glBindBuffer(GL_ARRAY_BUFFER, this->vbo_texture);
	this->gl->glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, (void *) 0);
	this->gl->glBindVertexArray(0);
}

void DrawableMesh::updateData(void) {
	auto vertices = this->mesh->getVertices();
	auto normals  = this->mesh->getVertexNormals();
	// we assume that the triangles and texture data do not change ...
	this->gl->glBindBuffer(GL_ARRAY_BUFFER, this->vbo_vertices);
	this->gl->glBufferData(GL_ARRAY_BUFFER, vertices.size() * 3 * sizeof(GLfloat), vertices.data(), GL_STATIC_DRAW);
	this->gl->glBindBuffer(GL_ARRAY_BUFFER, this->vbo_normals);
	this->gl->glBufferData(GL_ARRAY_BUFFER, normals.size() * 3 * sizeof(GLfloat), normals.data(), GL_STATIC_DRAW);
	// disable update for new draw call :
	this->should_update_on_next_draw = false;
}