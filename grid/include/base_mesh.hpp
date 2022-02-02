#ifndef BASE_MESH_HPP_
#define BASE_MESH_HPP_

#include <glm/glm.hpp>
#include <iostream>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/io.hpp>
#include <vector>

struct MeshDeformator;
namespace UITool {
    class Manipulator;
}

class BaseMesh : public QObject {
    Q_OBJECT
public:

    float scale;
    glm::mat4 transformation;

    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> texCoord;// These are normalised coordinates

    glm::vec3 bbMin;
    glm::vec3 bbMax;

    MeshDeformator * meshDeformator;// It will move the mesh points using certain strategies

    glm::vec3 getDimensions() const;
    int getIdxOfClosestPoint(const glm::vec3& p) const;

    BaseMesh();

    void updatebbox();
    std::vector<glm::vec3>& getMeshPositions();

    // Functions to interact with the mesh
    void movePoint(const glm::vec3& origin, const glm::vec3& target);
    void setNormalDeformationMethod();
    void setWeightedDeformationMethod(float radius);
    void selectPts(const glm::vec3& pt);
    void deselectAllPts();

    glm::vec3 getOrigin();
    glm::mat4 getModelTransformation();
    void setOrigin(const glm::vec3& origin);
    void setScale(float scale);

    virtual void computeNeighborhood() = 0;
    virtual void computeNormals() = 0;
    virtual ~BaseMesh(){};
};

#endif