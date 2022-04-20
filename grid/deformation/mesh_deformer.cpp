#include "mesh_deformer.hpp"
#include "../geometry/tetrahedral_mesh.hpp"
#include "../geometry/surface_mesh.hpp"
#include <algorithm>
#include "glm/gtx/string_cast.hpp"

NormalMethod::NormalMethod(BaseMesh * baseMesh) : MeshDeformer(baseMesh, DeformMethod::NORMAL) {}

void NormalMethod::movePoint(const glm::vec3& origin, const glm::vec3& target) {
    const glm::vec3 deplacement = target - origin;
    for(int i = 0; i < this->selectedPts.size(); ++i) {
        this->baseMesh->vertices[this->selectedPts[i]] += deplacement;
    }
}

void NormalMethod::movePoints(const std::vector<glm::vec3>& origins, const std::vector<glm::vec3>& targets) {
    for(int i = 0; i < origins.size(); ++i) {
        this->baseMesh->vertices[this->baseMesh->getIdxOfClosestPoint(origins[i])] = targets[i];
    }
}

/***/

ARAPMethod::ARAPMethod(SurfaceMesh * surfaceMesh) : MeshDeformer(dynamic_cast<BaseMesh*>(surfaceMesh), DeformMethod::ARAP){
    this->onSurfaceMesh = true;
    std::vector<Vec3D<float>> ptsAsVec3D;
    for(int i = 0; i < this->baseMesh->getNbVertices(); ++i) {
        glm::vec3 pt = surfaceMesh->getVertice(i);
        ptsAsVec3D.push_back(Vec3D(pt[0], pt[1], pt[2]));
        this->handles.push_back(false);
    }

    this->arap.clear();
    this->arap.init(ptsAsVec3D, surfaceMesh->getTriangles());
    this->arap.setHandles(this->handles);
}

void ARAPMethod::initARAP() {
    if(this->onSurfaceMesh) {
        std::vector<Vec3D<float>> ptsAsVec3D;
        for(int i = 0; i < this->baseMesh->getNbVertices(); ++i) {
            glm::vec3 pt = this->baseMesh->getVertice(i);
            ptsAsVec3D.push_back(Vec3D(pt[0], pt[1], pt[2]));
        }
        this->arap.clear();
        this->arap.init(ptsAsVec3D, dynamic_cast<SurfaceMesh*>(this->baseMesh)->getTriangles());
    }
}

ARAPMethod::ARAPMethod(BaseMesh * baseMesh) : MeshDeformer(baseMesh, DeformMethod::ARAP) {
    this->onSurfaceMesh = false;
    this->arap.clear();
    std::cout << "WARNING: trying to use ARAP deformation on GenericMesh, but ARAP only works on SurfaceMesh, thus the operation will be a [DIRECT] deformation" << std::endl;
}

void ARAPMethod::setHandle(int idx) {
    if(this->onSurfaceMesh) {
        this->handles[idx] = true;
    }
}

void ARAPMethod::unsetHandle(int idx) {
    if(this->onSurfaceMesh) {
        this->handles[idx] = false;
    }
}

void ARAPMethod::movePoint(const glm::vec3& origin, const glm::vec3& target) {
    const glm::vec3 deplacement = target - origin;
    for(int i = 0; i < this->selectedPts.size(); ++i)
        this->baseMesh->vertices[this->selectedPts[i]] += deplacement;

    if(this->onSurfaceMesh) {
        std::vector<Vec3D<float>> ptsAsVec3D;
        for(int i = 0; i < this->baseMesh->getNbVertices(); ++i) {
            glm::vec3 pt = this->baseMesh->getVertice(i);
            ptsAsVec3D.push_back(Vec3D(pt[0], pt[1], pt[2]));
        }
        this->arap.setHandles(this->handles);
        this->arap.compute_deformation(ptsAsVec3D);
        for(int i = 0; i < this->baseMesh->getNbVertices(); ++i)
            this->baseMesh->vertices[i] = glm::vec3(ptsAsVec3D[i][0], ptsAsVec3D[i][1],ptsAsVec3D[i][2]);
    }
}

void ARAPMethod::movePoints(const std::vector<glm::vec3>& origins, const std::vector<glm::vec3>& targets) {
    std::vector<int> idxInMesh;
    for(int i = 0; i < origins.size(); ++i) {
        idxInMesh.push_back(this->baseMesh->getIdxOfClosestPoint(origins[i]));
    }
    for(int i = 0; i < idxInMesh.size(); ++i) {
        this->baseMesh->vertices[idxInMesh[i]] = targets[i];
        if(!this->handles[idxInMesh[i]]) {
            std::cout << "ERROR for index: " << idxInMesh[i] << std::endl;
        }
    }

    if(this->onSurfaceMesh) {
        std::vector<Vec3D<float>> ptsAsVec3D;
        for(int i = 0; i < this->baseMesh->getNbVertices(); ++i) {
            glm::vec3 pt = this->baseMesh->getVertice(i);
            ptsAsVec3D.push_back(Vec3D(pt[0], pt[1], pt[2]));
        }
        this->arap.setHandles(this->handles);
        this->arap.compute_deformation(ptsAsVec3D);
        for(int i = 0; i < this->baseMesh->getNbVertices(); ++i)
            this->baseMesh->vertices[i] = glm::vec3(ptsAsVec3D[i][0], ptsAsVec3D[i][1],ptsAsVec3D[i][2]);
    }
}

void ARAPMethod::fitToPointList(const std::vector<int>& vertices, const std::vector<glm::vec3>& newPositions) {
    for(int i = 0; i < vertices.size(); ++i) {
        this->handles[vertices[i]] = true;
        this->baseMesh->vertices[vertices[i]] = newPositions[i];
    }

    if(this->onSurfaceMesh) {
        std::vector<Vec3D<float>> ptsAsVec3D;
        for(int i = 0; i < this->baseMesh->getNbVertices(); ++i) {
            glm::vec3 pt = this->baseMesh->getVertice(i);
            ptsAsVec3D.push_back(Vec3D(pt[0], pt[1], pt[2]));
        }
        this->arap.setHandles(this->handles);
        this->arap.compute_deformation(ptsAsVec3D);
        for(int i = 0; i < this->baseMesh->getNbVertices(); ++i)
            this->baseMesh->vertices[i] = glm::vec3(ptsAsVec3D[i][0], ptsAsVec3D[i][1],ptsAsVec3D[i][2]);
    }

    for(int i = 0; i < vertices.size(); ++i)
        this->handles[vertices[i]] = false;
    this->baseMesh->computeNormals();
    this->baseMesh->updatebbox();
}
