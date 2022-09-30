#ifndef VISUALISATION_MESHES_DRAWABLE_SURFACE_MESH_HPP_
#define VISUALISATION_MESHES_DRAWABLE_SURFACE_MESH_HPP_

#include "../geometry/surface_mesh.hpp"

#include "../../legacy/meshes/drawable/shaders.hpp"

#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFunctions>
#include <QOpenGLFunctions_3_2_Compatibility>
#include <QOpenGLFunctions_3_2_Core>

#include <glm/glm.hpp>
#include <glm/vector_relational.hpp>

#include <iostream>
#include <memory>
#include <vector>

#include <memory>
#include <vector>

class DrawableMesh {
public:
	~DrawableMesh() = default;

    void initialize(ShaderCompiler::GLFunctions* functions);

    void draw(GLfloat *proj_mat, GLfloat *view_mat, const glm::vec4& camera, const glm::vec3& planePosition);

	SurfaceMesh * mesh;
    glm::vec4 color;
    glm::vec3 lightPosition;

    void makeVAO();
    void updateData();
protected:

	GLuint program_handle_draw;

	GLuint vao;
	GLuint vbo_vertices;
	GLuint vbo_normals;
	GLuint vbo_indices;

	ShaderCompiler::GLFunctions* gl;

	bool should_update_on_next_draw;
};

#endif	  // VISUALISATION_MESHES_DRAWABLE_MESH_HPP_
