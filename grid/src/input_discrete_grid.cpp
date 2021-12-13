#include "../include/input_discrete_grid.hpp"

InputGrid::InputGrid(void) {
	this->gridName				= "defaultInputGrid";
	this->transform_gridToWorld = glm::mat4(1.f);
	this->transform_worldToGrid = glm::mat4(1.f);

	// By default, calling setModifiable() would bypass virtual dispatch, thus
	// calling InputGrid::setModifiable(), which did nothing. Se we set the
	// value here 'in stone' in order to prevent modifications from happening.
	this->modifiable = false;
}

InputGrid::~InputGrid() {
}

InputGrid& InputGrid::preAllocateImageData(sizevec3 dimensions) {
	this->gridDimensions = dimensions;
	std::size_t datasize = dimensions.x * dimensions.y * dimensions.z;
	this->data.resize(datasize);
	// Input grids have voxel sizes of 1, and their bounding box is equal to their dimensions :
	this->voxelDimensions = glm::vec3(1.f, 1.f, 1.f);
	this->boundingBox.setMin(glm::vec3(.0f));
	this->boundingBox.setMax(glm::vec3(static_cast<float>(dimensions.x), static_cast<float>(dimensions.y), static_cast<float>(dimensions.z)));

	return *this;
}

InputGrid& InputGrid::addImage(std::vector<DataType> imgData, std::size_t imgIndex) {
	if (imgIndex > this->gridDimensions.z) {
		std::cerr << "[ERROR] Tried to access image at depth of "
				  << imgIndex << " out of " << this->gridDimensions.z << '\n';
		return *this;
	}
	// Overrides any data previously here, and assumes the image is the same
	// resolution as the grid it's inserted into :
	std::size_t startIdx = this->gridDimensions.x * this->gridDimensions.y * imgIndex;
	// Copy the data :
	std::copy(imgData.begin(), imgData.end(), this->data.begin() + startIdx);
	// data copied, but we should not update the data bounding box.
	return *this;
}

InputGrid& InputGrid::setGrid(std::vector<DataType> imgData, sizevec3 dimensions) {
	this->data.clear();
	this->data.resize(dimensions.x * dimensions.y * dimensions.z);
	std::copy(imgData.begin(), imgData.end(), this->data.begin());
	// Set the grid's dimensions :
	this->gridDimensions = dimensions;
	// Set the bounding box's dimensions :
	this->boundingBox.setMin(glm::vec3(.0f));
	this->boundingBox.setMax(glm::vec3(static_cast<float>(dimensions.x), static_cast<float>(dimensions.y), static_cast<float>(dimensions.z)));
	// Set the bounding box for data loaded :
	this->recomputeBoundingBox(this->dataThreshold);

	return *this;
}

InputGrid& InputGrid::setModifiable(bool b) {
	return *this;
}

InputGrid& InputGrid::setResolution(sizevec3 newRes) {
	return *this;
}

InputGrid& InputGrid::setBoundingBox(bbox_t renderWindow) {
	return *this;
}