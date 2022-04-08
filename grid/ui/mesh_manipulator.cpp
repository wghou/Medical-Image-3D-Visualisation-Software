#include "mesh_manipulator.hpp"
#include "manipulator.hpp"
#include "../deformation/mesh_deformer.hpp"
#include "../deformation/cage_surface_mesh.hpp"


namespace UITool {
    
    float MeshManipulator::getManipulatorSize(bool side) {
        if(this->mesh)
            if(side)
                return std::max(std::max(this->mesh->getDimensions()[0], this->mesh->getDimensions()[1]), this->mesh->getDimensions()[2])/130.;     
            else
                return std::min(std::min(this->mesh->getDimensions()[0], this->mesh->getDimensions()[1]), this->mesh->getDimensions()[2])/130.;     
        else
            return 10.;
    }

	DirectManipulator::DirectManipulator(BaseMesh * mesh, const std::vector<glm::vec3>& positions): MeshManipulator(mesh) {
        this->kid_manip = nullptr;
        this->manipulators.reserve(positions.size());
		for (int i = 0; i < positions.size(); ++i) {
			this->manipulators.push_back(Manipulator(positions[i]));
            //QObject::connect(&(this->manipulators[i]), &Manipulator::enterAtRangeForGrab, this, &DirectManipulator::displayManipulator);
            //QObject::connect(&(this->manipulators[i]), &Manipulator::exitFromRangeForGrab, this, &DirectManipulator::hideManipulator);

            QObject::connect(&(this->manipulators[i]), &Manipulator::mouseRightButtonPressed, this, &DirectManipulator::selectManipulator);
            QObject::connect(&(this->manipulators[i]), &Manipulator::mouseRightButtonReleasedAndCtrlIsNotPressed, this, &DirectManipulator::deselectManipulator);
            QObject::connect(&(this->manipulators[i]), &Manipulator::isManipulated, this, &DirectManipulator::moveManipulator);

            this->selectedManipulators.push_back(false);
            this->manipulatorsToDisplay.push_back(true);
			this->manipulators[i].setCustomConstraint();
            this->manipulators[i].enable();
		}

        QObject::connect(&this->selection, &Selection::isSelecting, this, &DirectManipulator::checkSelectedManipulators);
	}

    void DirectManipulator::checkSelectedManipulators() {
        this->deselectManipulator(nullptr);
        std::vector<glm::vec3> positions;
        this->getAllPositions(positions);
        std::vector<bool> inSelection = this->selection.areInSelection(positions);
        for(int i = 0; i < inSelection.size(); ++i) {
            if(inSelection[i]) {
                this->selectManipulator(&this->manipulators[i]);
            }
        }
    }

	void DirectManipulator::setAllManipulatorsPosition(const std::vector<glm::vec3>& positions) {
		if (positions.size() == this->manipulators.size()) {
			for (int i = 0; i < this->manipulators.size(); ++i) {
				this->manipulators[i].setManipPosition(positions[i]);
				this->manipulators[i].setLastPosition(positions[i]);
			}
		} else {
			std::cerr << "WARNING: try to set [" << this->manipulators.size() << "] manipulators positions with a position vector of size [" << positions.size() << "]" << std::endl;
		}
	}

    void DirectManipulator::getAllPositions(std::vector<glm::vec3>& positions) {
		for (int i = 0; i < this->manipulators.size(); ++i) {
            positions.push_back(this->manipulators[i].getManipPosition());
		}
    }

    void DirectManipulator::getManipulatorsToDisplay(std::vector<bool>& toDisplay) const {
		for (int i = 0; i < this->manipulatorsToDisplay.size(); ++i) {
            toDisplay.push_back(this->manipulatorsToDisplay[i]);
        }
    }

    void DirectManipulator::getManipulatorsState(std::vector<State>& states) const {
        states.clear();
        for(int i = 0; i < this->manipulatorsToDisplay.size(); ++i) {
            State currentState = State::NONE;
            if(this->manipulators[i].isAtRangeForGrab)
                currentState = State::AT_RANGE;
            if(this->selectedManipulators[i])
                currentState = State::AT_RANGE;
            if(this->manipulators[i].isSelected)
                currentState = State::MOVE;
            states.push_back(currentState);
        }
    }

    void DirectManipulator::displayManipulator(Manipulator * manipulatorToDisplay) {
        // Since vectors are organized sequentially, you can get an index by subtracting pointer to initial element 
        // from the pointer to the element you search
        ptrdiff_t index = manipulatorToDisplay - &(this->manipulators[0]);
        this->manipulatorsToDisplay[index] = true;
    }

    void DirectManipulator::hideManipulator(Manipulator * manipulatorToDisplay) {
        if(manipulatorToDisplay->isSelected)
            return;
        // Since vectors are organized sequentially, you can get an index by subtracting pointer to initial element 
        // from the pointer to the element you search
        ptrdiff_t index = manipulatorToDisplay - &(this->manipulators[0]);
        this->manipulatorsToDisplay[index] = false;
    }

    void DirectManipulator::moveManipulator(Manipulator * manipulator) {
        this->mesh->movePoint(manipulator->lastPosition, manipulator->getManipPosition());
        Q_EMIT needSendTetmeshToGPU();
    }

    void DirectManipulator::selectManipulator(Manipulator * manipulator) {
        ptrdiff_t index = manipulator - &(this->manipulators[0]);
        if(!this->selectedManipulators[index]) {
            this->mesh->selectPts(manipulator->getManipPosition());
            this->selectedManipulators[index] = true;
        }
        std::cout << "Selection vertex: [" << index << "]" << std::endl;
    }

    void DirectManipulator::deselectManipulator(Manipulator * manipulator) {
        this->mesh->deselectAllPts();
        for(int i = 0; i < this->selectedManipulators.size(); ++i) {
            this->selectedManipulators[i] = false;
        }
    }

    void DirectManipulator::keyPressed(QKeyEvent* e) {};

    void DirectManipulator::keyReleased(QKeyEvent* e) {};

    void DirectManipulator::mousePressed(QMouseEvent*) {};

    void DirectManipulator::mouseReleased(QMouseEvent*) {};

    /***/

	FreeManipulator::FreeManipulator(BaseMesh * mesh, const std::vector<glm::vec3>& positions): MeshManipulator(mesh), manipulator(Manipulator(glm::vec3(0., 0., 0.))) {
		this->active = false;
        this->kid_manip = nullptr;
        QObject::connect(&(this->manipulator), &Manipulator::mouseRightButtonPressed, this, &FreeManipulator::selectManipulator);
        QObject::connect(&(this->manipulator), &Manipulator::mouseRightButtonReleasedAndCtrlIsNotPressed, this, &FreeManipulator::deselectManipulator);
        QObject::connect(&(this->manipulator), &Manipulator::isManipulated, this, &FreeManipulator::moveManipulator);

        QObject::connect(this, &FreeManipulator::rayIsCasted, this, &FreeManipulator::addManipulatorFromRay);
	}

    void FreeManipulator::setActivation(bool isActive) {
        this->active = isActive;
        if (this->active) {
            this->manipulator.setCustomConstraint();
        } else {
            this->manipulator.lockPosition();
        }
    }

    void FreeManipulator::addManipulator(const glm::vec3& position) {
        this->manipulator.setManipPosition(position);
        this->manipulator.setLastPosition(position);
        this->setActivation(true);
    }

	void FreeManipulator::setAllManipulatorsPosition(const std::vector<glm::vec3>& positions) {
        //Do nothing, because we don't want to move these manipulator
	}

    void FreeManipulator::getAllPositions(std::vector<glm::vec3>& positions) {
        positions.push_back(this->manipulator.getManipPosition());
    }

    void FreeManipulator::getManipulatorsToDisplay(std::vector<bool>& toDisplay) const {
        toDisplay.push_back(this->active);
    }

    void FreeManipulator::getManipulatorsState(std::vector<State>& states) const {
        states.clear();
        State currentState = State::NONE;
        if(this->manipulator.isAtRangeForGrab)
            currentState = State::AT_RANGE;
        if(this->manipulator.isSelected)
            currentState = State::MOVE;
        states.push_back(currentState);
    }

    void FreeManipulator::moveManipulator(Manipulator * manipulator) {
        this->mesh->movePoint(manipulator->lastPosition, manipulator->getManipPosition());
        Q_EMIT needSendTetmeshToGPU();
    }

    void FreeManipulator::selectManipulator(Manipulator * manipulator) {
        this->mesh->selectPts(manipulator->getManipPosition());
    }

    void FreeManipulator::deselectManipulator(Manipulator * manipulator) {
        this->mesh->deselectAllPts();
        this->setActivation(false);
    }

    void FreeManipulator::addManipulatorFromRay(const glm::vec3& origin, const glm::vec3& direction, uint16_t minValue, uint16_t maxValue, const glm::vec3& planePos) {
        glm::vec3 manipulatorPosition;
        if(this->mesh->getPositionOfRayIntersection(origin, direction, minValue, maxValue, planePos, manipulatorPosition))
            this->addManipulator(manipulatorPosition);
    }

    void FreeManipulator::keyPressed(QKeyEvent* e) {}

    void FreeManipulator::keyReleased(QKeyEvent* e) {
        if(e->key() == Qt::Key_Q && !e->isAutoRepeat()) {
            setActivation(false);
        }
    }

    void FreeManipulator::mousePressed(QMouseEvent*) {}

    void FreeManipulator::mouseReleased(QMouseEvent*) {}

    /***/

	PositionManipulator::PositionManipulator(BaseMesh * mesh, const std::vector<glm::vec3>& positions): MeshManipulator(mesh) {
        this->selection.disable();
        this->evenMode = false;
        this->kid_manip = new RotationManipulator();
        this->kid_manip->setOrigine(qglviewer::Vec(mesh->getOrigin()[0], mesh->getOrigin()[1], mesh->getOrigin()[2]));
        for(int i = 0; i < this->mesh->vertices.size(); ++i) {
            this->kid_manip->addPoint(i, qglviewer::Vec(this->mesh->vertices[i][0], this->mesh->vertices[i][1], this->mesh->vertices[i][2]));
        }
        QObject::connect(this->kid_manip, &RotationManipulator::moved, this, [this]() {this->moveManipulator(nullptr);});
	}

	void PositionManipulator::setAllManipulatorsPosition(const std::vector<glm::vec3>& positions) {
        //Do nothing, because we don't want to move these manipulator
	}

    void PositionManipulator::getAllPositions(std::vector<glm::vec3>& positions) {
    }

    void PositionManipulator::getManipulatorsToDisplay(std::vector<bool>& toDisplay) const {
    }

    void PositionManipulator::getManipulatorsState(std::vector<State>& states) const {
    }

    void PositionManipulator::moveManipulator(Manipulator * manipulator) {
        for(int i = 0; i < this->mesh->vertices.size(); ++i) {
            qglviewer::Vec deformedPoint;
            int trueIndex;
            this->kid_manip->getTransformedPoint(i, trueIndex, deformedPoint);
            this->mesh->vertices[i] = glm::vec3(deformedPoint[0], deformedPoint[1], deformedPoint[2]);
            this->mesh->computeNormals();
            this->mesh->updatebbox();
        }
        Q_EMIT needSendTetmeshToGPU();
    }

    void PositionManipulator::keyPressed(QKeyEvent* e) {
        if(e->modifiers() == Qt::ControlModifier && !e->isAutoRepeat()) {
            this->evenMode = true;
            std::cout << "Enter even mode" << std::endl;
            this->kid_manip->evenMode = true;
        }
    }

    void PositionManipulator::keyReleased(QKeyEvent* e) {
        std::cout << "Exit even mode" << std::endl;
        this->evenMode = false;
        this->kid_manip->evenMode = false;
    }

    void PositionManipulator::mousePressed(QMouseEvent* e) {}

    void PositionManipulator::mouseReleased(QMouseEvent* e) {
        emit needRedraw();
    }

    PositionManipulator::~PositionManipulator() {
        delete this->kid_manip;
    }

    /***/

	CompManipulator::CompManipulator(BaseMesh * mesh, const std::vector<glm::vec3>& positions): MeshManipulator(mesh) {
        this->hasAMeshToRegister = false;
        this->isOnSelectionMode = false;
        this->isSelectingFirstPoint = false;
        this->isSelectingSecondPoint = false;
        this->oneManipulatorWasAlreadyAdded = false;
        this->oneManipulatorIsAtRangeForGrab = false;
        this->currentPairToSelect = -1;
        this->kid_manip = nullptr;
        QObject::connect(this, &CompManipulator::pointIsClickedInPlanarViewer, this, &CompManipulator::addManipulator);
        QObject::connect(this, &CompManipulator::rayIsCasted, this, &CompManipulator::addManipulatorFromRay);
	}

    void CompManipulator::clearSelectedPoints() {
        int nbOfPointsToUndo = this->selectedPoints.size();
        std::cout << nbOfPointsToUndo << " points deleted" << std::endl;
        for(int nb = 0; nb < nbOfPointsToUndo; ++nb) {
            this->manipulatorsState[this->selectedPoints.back().first] = 0;
            this->manipulators.pop_back();
            this->manipulatorsToDisplay.pop_back();
            this->manipulatorsIsOnMesh.pop_back();
            this->manipulatorsState.pop_back();
            this->selectedPoints.pop_back();
        }
    }

    void CompManipulator::undo() {

        if(this->previousNumberOfSelectedPoints.size() == 0) {
            std::cout << "Nothing to undo" << std::endl;
            return;
        }

        int nbOfPointsToUndo = this->selectedPoints.size();
        if(this->previousNumberOfSelectedPoints.size() > 1) {
            int prevNumberOfSelectedPoint = this->previousNumberOfSelectedPoints[this->previousNumberOfSelectedPoints.size()-2];
            int nbSelectedPoint = this->selectedPoints.size();
            nbOfPointsToUndo = nbSelectedPoint - prevNumberOfSelectedPoint;
        }

        std::cout << nbOfPointsToUndo << " points deleted" << std::endl;
        for(int nb = 0; nb < nbOfPointsToUndo; ++nb) {
            this->manipulatorsState[this->selectedPoints.back().first] = 0;
            this->manipulators.pop_back();
            this->manipulatorsToDisplay.pop_back();
            this->manipulatorsIsOnMesh.pop_back();
            this->manipulatorsState.pop_back();
            this->selectedPoints.pop_back();
        }

        for(int i = 0; i < this->meshToRegister->vertices.size(); ++i) {
            this->meshToRegister->vertices[i] = this->previousPositions.back()[i];
            this->manipulators[i].setManipPosition(this->meshToRegister->vertices[i]);
        }
        this->previousPositions.pop_back();
        this->meshToRegister->computeNormals();
        this->meshToRegister->updatebbox();
    }

    void CompManipulator::assignPreviousSelectedPoints(const std::vector<std::pair<int, std::pair<int, glm::vec3>>>& previousSelectedPoints, const std::vector<std::vector<glm::vec3>>& previousPositions, std::vector<int> previousNumberOfSelectedPoints) {
        this->previousPositions = previousPositions;
        this->previousNumberOfSelectedPoints = previousNumberOfSelectedPoints;
        for(int i = 0; i < previousSelectedPoints.size(); ++i) {
            this->selectedPoints.push_back(std::make_pair(previousSelectedPoints[i].first, std::make_pair(previousSelectedPoints[i].second.first, previousSelectedPoints[i].second.second)));
            int addedIndex = this->selectedPoints.back().second.first;
            if(addedIndex >= this->manipulators.size()) {
                this->manipulators.push_back(Manipulator(this->selectedPoints.back().second.second));
                this->manipulators.back().setCustomConstraint();
                this->manipulatorsIsOnMesh.push_back(false);
                this->manipulatorsState.push_back(7);
                this->manipulatorsToDisplay.push_back(true);
                if(addedIndex != this->manipulators.size() - 1) {
                    std::cout << "Error in selected points, this will bug !!" << std::endl;
                    std::cout << "Added index: " << addedIndex << std::endl;
                    std::cout << "Size: " << this->manipulators.size() << std::endl;
                }
            }
        }
    }

    void CompManipulator::switchToSelectionMode() {
        if(!hasAMeshToRegister) {
            this->displayErrorNoMeshAssigned();
            return;
        }
        this->isOnSelectionMode = true;
        this->isSelectingFirstPoint = true;
        this->currentPairToSelect = this->selectedPoints.size();
        this->selectedPoints.push_back(std::make_pair(-1, std::make_pair(-1, glm::vec3(0., 0., 0.))));
        for(int i = 0; i < this->manipulators.size(); ++i) {
            if(this->manipulatorsIsOnMesh[i]) {
                this->manipulatorsState[i] = 2;
                this->manipulatorsToDisplay[i] = true;
            } else {
                this->manipulatorsToDisplay[i] = false;
            }
            this->manipulators[i].lockPosition();
        }
    }

    void CompManipulator::assignMeshToRegister(BaseMesh * meshToRegister) {
        this->meshToRegister = meshToRegister;
        this->hasAMeshToRegister = true;

        std::vector<glm::vec3> vertices = meshToRegister->vertices;
        this->manipulators.reserve(vertices.size()*2.);
		for (int i = 0; i < vertices.size(); ++i) {
			this->manipulators.push_back(Manipulator(vertices[i]));
            this->manipulatorsIsOnMesh.push_back(true);
            QObject::connect(&(this->manipulators[i]), &Manipulator::enterAtRangeForGrab, this, [this](){this->oneManipulatorIsAtRangeForGrab = true;});
            QObject::connect(&(this->manipulators[i]), &Manipulator::exitFromRangeForGrab, this, [this](){this->oneManipulatorIsAtRangeForGrab = false;});
            QObject::connect(&(this->manipulators[i]), &Manipulator::mouseRightButtonPressed, this, &CompManipulator::selectManipulator);

            this->manipulatorsToDisplay.push_back(false);
            this->manipulatorsState.push_back(0.);
			this->manipulators[i].lockPosition();
            this->manipulators[i].enable();
		}
    }

    void CompManipulator::addManipulator(const glm::vec3& position) {
        if(!hasAMeshToRegister) {
            this->displayErrorNoMeshAssigned();
            return;
        }
        if(this->isSelectingFirstPoint) {
            std::cout << "Please first select a point on the mesh to register" << std::endl;
        } 

        if(this->isSelectingSecondPoint) {
            if(this->oneManipulatorWasAlreadyAdded && !this->oneManipulatorIsAtRangeForGrab) {
                this->manipulators.back().setManipPosition(position);
                this->manipulators.back().setCustomConstraint();
            } else {
                this->manipulators.push_back(Manipulator(position));
                this->manipulators.back().setCustomConstraint();
                this->manipulatorsIsOnMesh.push_back(false);
                this->manipulatorsState.push_back(5);
                this->manipulatorsToDisplay.push_back(true);
                this->oneManipulatorWasAlreadyAdded = true;
            }
            this->selectedPoints[this->currentPairToSelect].second.first = this->manipulators.size() - 1;
            this->selectedPoints[this->currentPairToSelect].second.second = this->manipulators.back().getManipPosition();
        }
    }

    void CompManipulator::validate() {
        if(this->selectedPoints[this->currentPairToSelect].first == -1) {
            std::cout << "No point selected on the mesh" << std::endl;
        }

        if(this->selectedPoints[this->currentPairToSelect].second.first == -1) {
            std::cout << "No point selected on the grid" << std::endl;
        }

        this->isOnSelectionMode = false;
        this->isSelectingFirstPoint = false;
        this->isSelectingSecondPoint = false;
        this->oneManipulatorWasAlreadyAdded = false;
        this->currentPairToSelect = -1;

        for(int i = 0; i < this->manipulators.size(); ++i) {
            if(this->manipulatorsIsOnMesh[i]) {
                this->manipulatorsState[i] = 0;
            } else {
                this->manipulatorsState[i] = 6;
            }
        }

        for(int i = 0; i < this->selectedPoints.size(); ++i) {
            this->manipulatorsState[this->selectedPoints[i].first] = 1;
            this->manipulatorsState[this->selectedPoints[i].second.first] = 7;
            this->manipulatorsToDisplay[this->selectedPoints[i].first] = true;
            this->manipulatorsToDisplay[this->selectedPoints[i].second.first] = true;
        }
    }

    void CompManipulator::apply() {
        if(this->isOnSelectionMode) {
            std::cout << "You are currently selected some points.\n Please Validate your selection before apply the transformation." << std::endl;
            return;
        }
        std::vector<int> verticesToFit;
        std::vector<glm::vec3> newPositions;
        std::cout << "Selected pairs are:" << std::endl;
        for(int i = 0; i < this->selectedPoints.size(); ++i) {
            std::cout << "Mesh: " << this->selectedPoints[i].first << std::endl;
            std::cout << "Grid: " << this->selectedPoints[i].second.first << std::endl;
            verticesToFit.push_back(this->selectedPoints[i].first);
            newPositions.push_back(this->selectedPoints[i].second.second);
        }
        this->meshToRegister->setARAPDeformationMethod();
        ARAPMethod * deformer = dynamic_cast<ARAPMethod*>(this->meshToRegister->meshDeformer);
        if(deformer) {
            this->previousPositions.push_back(this->meshToRegister->vertices);
            deformer->fitToPointList(verticesToFit, newPositions);
        }
    }

    void CompManipulator::addManipulatorFromRay(const glm::vec3& origin, const glm::vec3& direction, uint16_t minValue, uint16_t maxValue, const glm::vec3& planePos) {
        glm::vec3 manipulatorPosition;
        if(this->mesh->getPositionOfRayIntersection(origin, direction, minValue, maxValue, planePos, manipulatorPosition)) {
            this->addManipulator(manipulatorPosition);
        }
    }

	void CompManipulator::setAllManipulatorsPosition(const std::vector<glm::vec3>& positions) {
	}

    void CompManipulator::getAllPositions(std::vector<glm::vec3>& positions) {
        positions.clear();
		for (int i = 0; i < this->manipulators.size(); ++i) {
            positions.push_back(this->manipulators[i].getManipPosition());
		}
    }

    void CompManipulator::getManipulatorsToDisplay(std::vector<bool>& toDisplay) const {
        toDisplay.clear();
		for (int i = 0; i < this->manipulatorsToDisplay.size(); ++i) {
            toDisplay.push_back(this->manipulatorsToDisplay[i]);
        }
    }

    void CompManipulator::getManipulatorsState(std::vector<State>& states) const {
        states.clear();
        for(int i = 0; i < this->manipulatorsState.size(); ++i) {
            State currentState = State::NONE;
            switch(this->manipulatorsState[i]) {
                // 0: not assigned to a point (neutral)(on mesh)
                // 1: assigned to a point (neutral)(on mesh)
                // 2: waiting to be selected (on mesh)
                // 3: selected, waiting for second point (on mesh)
                // 5: waiting for validation (on grid)
                // 7: assigned to a point (neutral)(on grid)
                case 0:
                    currentState = State::NONE;
                    break;
                case 1:
                    currentState = State::SELECTED;
                    break;
                case 2:
                    currentState = State::WAITING;
                    break;
                case 3:
                    currentState = State::AT_RANGE;
                    break;
                case 5:
                    currentState = State::MOVE;
                    break;
                case 7:
                    currentState = State::LOCK;
                    break;
            }
            if(this->manipulators[i].isAtRangeForGrab)
                currentState = State::AT_RANGE;
            states.push_back(currentState);
        }
    }

    void CompManipulator::selectManipulator(Manipulator * manipulator) {
        if(!hasAMeshToRegister) {
            this->displayErrorNoMeshAssigned();
            return;
        }
        ptrdiff_t index = manipulator - &(this->manipulators[0]);
        if(this->isSelectingFirstPoint) {
            if(!this->manipulatorsIsOnMesh[index]) {
                std::cout << "Select a point on the mesh please" << std::endl;
                return;
            }
            for(int i = 0; i < this->manipulators.size(); ++i) {
                if(this->manipulatorsIsOnMesh[i]) {
                    this->manipulatorsState[i] = 4;// Not selected
                    this->manipulatorsToDisplay[i] = false;
                }
            }

            this->manipulatorsState[index] = 3;// Selected
            this->manipulatorsToDisplay[index] = true;// Selected
            this->selectedPoints[this->currentPairToSelect].first = index;
            this->isSelectingFirstPoint = false;
            this->isSelectingSecondPoint = true;
            return;
        } 

        if(this->isSelectingSecondPoint) {
            if(this->manipulatorsIsOnMesh[index]) {
                std::cout << "A marker in the mesh has already been selected, please select a point on the grid now" << std::endl;
                return;
            }

            if(index == this->manipulators.size() -1) {
                this->selectedPoints[this->currentPairToSelect].second.first = index;
                this->selectedPoints[this->currentPairToSelect].second.second = this->manipulators[index].getManipPosition();
            } else {
                std::cout << "This marker has already been assigned" << std::endl;
            }
        }
    }

    void CompManipulator::deselectManipulator(Manipulator * manipulator) {
    }

    void CompManipulator::displayErrorNoMeshAssigned() {
        std::cout << "Error: no mesh assigned" << std::endl;
    }

    void CompManipulator::keyPressed(QKeyEvent* e) {}

    void CompManipulator::keyReleased(QKeyEvent* e) {}

    void CompManipulator::mousePressed(QMouseEvent* e) {}

    void CompManipulator::mouseReleased(QMouseEvent* e) {}

    /***/

	ARAPManipulator::ARAPManipulator(BaseMesh * mesh, const std::vector<glm::vec3>& positions): MeshManipulator(mesh) {
        this->kid_manip = new RotationManipulator();
        QObject::connect(this->kid_manip, &RotationManipulator::moved, this, [this]() {this->moveKidManip();});
        this->kid_manip->disable();
        this->moveMode = true;
        this->addMode = true;
        this->manipulators.reserve(positions.size());
		for (int i = 0; i < positions.size(); ++i) {
			this->manipulators.push_back(Manipulator(positions[i]));
            QObject::connect(&(this->manipulators[i]), &Manipulator::enterAtRangeForGrab, this, &ARAPManipulator::displayManipulator);
            QObject::connect(&(this->manipulators[i]), &Manipulator::exitFromRangeForGrab, this, &ARAPManipulator::hideManipulator);

            QObject::connect(&(this->manipulators[i]), &Manipulator::mouseRightButtonPressed, this, &ARAPManipulator::selectManipulator);
            QObject::connect(&(this->manipulators[i]), &Manipulator::mouseRightButtonReleased, this, &ARAPManipulator::deselectManipulator);
            QObject::connect(&(this->manipulators[i]), &Manipulator::isManipulated, this, &ARAPManipulator::moveManipulator);

            this->manipulatorsToDisplay.push_back(false);
			this->manipulators[i].setCustomConstraint();
            this->manipulators[i].enable();
            this->selectedManipulators.push_back(false);

            this->handles.push_back(false);
		}
        QObject::connect(&this->selection, &Selection::isSelecting, this, &ARAPManipulator::checkSelectedManipulators);
        QObject::connect(&this->selection, &Selection::enterSelectionMode, this, [this](){
                if(this->kid_manip->isEnable) {
                    this->kid_manip->disable();
                }
                this->initializeSelection();
        });
        //QObject::connect(&this->selection, &Selection::exitSelectionMode, this, [this](){
        //        this->initializeSelection();
        //});
        QObject::connect(&this->selection, &Selection::beginSelection, this, [this](){this->initializeSelection();});
        QObject::connect(&this->selection, &Selection::endSelection, this, [this](){
                if(!this->kid_manip->isEnable) {
                    this->kid_manip->enable();
                }
                glm::vec3 glmMean = this->getMeanPositionSelectedManipulators();
                this->kid_manip->setOrigine(qglviewer::Vec(glmMean[0], glmMean[1], glmMean[2]));
                Q_EMIT needChangeKidManipulatorRadius(glm::length(this->selectionMax - this->selectionMin));

                this->setLockAllManipulators(false);
                for(int i = 0; i < this->selectedManipulatorsIdx.size(); ++i) {
                    glm::vec3 glmManipPos = this->manipulators[this->selectedManipulatorsIdx[i]].getManipPosition();
                    this->kid_manip->addPoint(i, qglviewer::Vec(glmManipPos[0], glmManipPos[1], glmManipPos[2]));
                }
         });
	}

    glm::vec3 ARAPManipulator::getMeanPositionSelectedManipulators() {
        glm::vec mean = glm::vec3(0., 0., 0.);
        for(int i = 0; i < this->selectedManipulatorsIdx.size(); ++i) {
            mean += this->manipulators[this->selectedManipulatorsIdx[i]].getManipPosition();
        }
        return mean/float(this->selectedManipulatorsIdx.size());
    }

    void ARAPManipulator::initializeSelection() {
        std::cout << "Initialize the selection" << std::endl;
        if(this->kid_manip->isEnable) {
            this->kid_manip->disable();
        }
        if(this->moveMode) {
            this->selection.setColor(glm::vec4(1.,0., .0, 0.5));
        } else {
            this->selection.setColor(glm::vec4(0.5, 0.5, 0.5, 0.5));
        }
        this->setLockAllManipulators(true);
        this->resetMinAndMax(); 
        this->mesh->deselectAllPts(); 
        this->selectedManipulatorsIdx.clear();
        std::fill(this->selectedManipulators.begin(), this->selectedManipulators.end(), false);
        ARAPMethod * deformer = dynamic_cast<ARAPMethod*>(this->mesh->meshDeformer);
        if(deformer) {
            deformer->initARAP();
        }
    }

    void ARAPManipulator::moveKidManip() {
        std::vector<glm::vec3> originalPoints;
        std::vector<glm::vec3> targetPoints;
        for(int i = 0; i < this->selectedManipulatorsIdx.size(); ++i) {
            qglviewer::Vec deformedPoint;
            int trueIndex;
            this->kid_manip->getTransformedPoint(i, trueIndex, deformedPoint);
            if(!std::isnan(deformedPoint[0]) && !std::isnan(deformedPoint[1]) && !std::isnan(deformedPoint[2])) {
                originalPoints.push_back(this->manipulators[this->selectedManipulatorsIdx[i]].getManipPosition());
                //originalPoints.push_back(this->mesh->vertices[this->selectedManipulatorsIdx[i]]);
                targetPoints.push_back(glm::vec3(deformedPoint[0], deformedPoint[1], deformedPoint[2]));
            }
            //this->mesh->vertices[this->selectedManipulatorsIdx[i]] = glm::vec3(deformedPoint[0], deformedPoint[1], deformedPoint[2]);
        }
        this->mesh->movePoints(originalPoints, targetPoints);
        Q_EMIT needSendTetmeshToGPU();
    }

    void ARAPManipulator::checkSelectedManipulators() {
        std::vector<glm::vec3> positions;
        this->getAllPositions(positions);
        std::vector<bool> inSelection = this->selection.areInSelection(positions);
        for(int i = 0; i < inSelection.size(); ++i) {
            if(inSelection[i]) {
                this->selectManipulator(&this->manipulators[i]);
            }
        }
    }

	void ARAPManipulator::setAllManipulatorsPosition(const std::vector<glm::vec3>& positions) {
		if (positions.size() == this->manipulators.size()) {
			for (int i = 0; i < this->manipulators.size(); ++i) {
				this->manipulators[i].setManipPosition(positions[i]);
				this->manipulators[i].setLastPosition(positions[i]);
			}
		} else {
			std::cerr << "WARNING: try to set [" << this->manipulators.size() << "] manipulators positions with a position vector of size [" << positions.size() << "]" << std::endl;
		}
	}

    void ARAPManipulator::getAllPositions(std::vector<glm::vec3>& positions) {
		for (int i = 0; i < this->manipulators.size(); ++i) {
            positions.push_back(this->manipulators[i].getManipPosition());
		}
    }

    void ARAPManipulator::getManipulatorsToDisplay(std::vector<bool>& toDisplay) const {
		for (int i = 0; i < this->manipulatorsToDisplay.size(); ++i) {
            //toDisplay.push_back(this->manipulatorsToDisplay[i]);
            toDisplay.push_back(true);
        }
    }

    void ARAPManipulator::getManipulatorsState(std::vector<State>& states) const {
        states.clear();
        for(int i = 0; i < this->manipulatorsToDisplay.size(); ++i) {
            State currentState = State::NONE;
            if(this->handles[i])
                currentState = State::LOCK;
            if(this->manipulators[i].isAtRangeForGrab)
                currentState = State::AT_RANGE;
            if(this->selectedManipulators[i])
                currentState = State::AT_RANGE;
            if(this->manipulators[i].isSelected)
                currentState = State::MOVE;
            states.push_back(currentState);
        }
    }

    void ARAPManipulator::displayManipulator(Manipulator * manipulatorToDisplay) {
        ptrdiff_t index = manipulatorToDisplay - &(this->manipulators[0]);
        this->manipulatorsToDisplay[index] = true;
    }

    void ARAPManipulator::hideManipulator(Manipulator * manipulatorToDisplay) {
        if(manipulatorToDisplay->isSelected)
            return;
        ptrdiff_t index = manipulatorToDisplay - &(this->manipulators[0]);
        if(!this->handles[index])
            this->manipulatorsToDisplay[index] = false;
    }

    void ARAPManipulator::moveManipulator(Manipulator * manipulator) {
        if(!moveMode)
            return;
        ARAPMethod * deformer = dynamic_cast<ARAPMethod*>(this->mesh->meshDeformer);
        if(!deformer) {
            std::cout << "WARNING: ARAP manipulator can be used only with the ARAP deformer !" << std::endl;
            return;
        }
        this->mesh->movePoint(manipulator->lastPosition, manipulator->getManipPosition());
        Q_EMIT needSendTetmeshToGPU();
    }

    void ARAPManipulator::selectManipulator(Manipulator * manipulator) {
        ptrdiff_t index = manipulator - &(this->manipulators[0]);
        if(this->moveMode) {
            if(!this->selectedManipulators[index]) {
                this->mesh->selectPts(manipulator->getManipPosition());
                this->selectedManipulators[index] = true;
                selectedManipulatorsIdx.push_back(index);

                glm::vec3 glmManipPos = manipulator->getManipPosition();
                for(int j = 0; j < 3; ++j) {
                    if(this->selectionMin[j] > glmManipPos[j])
                        this->selectionMin[j] = glmManipPos[j];
                    if(this->selectionMax[j] < glmManipPos[j])
                        this->selectionMax[j] = glmManipPos[j];
                }
            }
        } else {
            ARAPMethod * deformer = dynamic_cast<ARAPMethod*>(this->mesh->meshDeformer);
            if(!deformer) {
                std::cout << "WARNING: ARAP manipulator can be used only with the ARAP deformer !" << std::endl;
                return;
            }
            if(!this->selection.isSelected) {
                if(!this->handles[index]) {
                    deformer->setHandle(index);
                    this->handles[index] = true;
                } else {
                    deformer->unsetHandle(index);
                    this->handles[index] = false;
                }
            } else {
                if(this->addMode) {
                    if(!this->handles[index]) {
                        deformer->setHandle(index);
                        this->handles[index] = true;
                    }
                } else {
                    if(this->handles[index]) {
                        deformer->unsetHandle(index);
                        this->handles[index] = false;
                    }
                }
            }
        }

    }

    void ARAPManipulator::deselectManipulator(Manipulator * manipulator) {
        if(this->moveMode) {
            this->mesh->deselectAllPts();
            std::fill(this->selectedManipulators.begin(), this->selectedManipulators.end(), false);
            this->selectedManipulatorsIdx.clear();
            ARAPMethod * deformer = dynamic_cast<ARAPMethod*>(this->mesh->meshDeformer);
            if(deformer) {
                deformer->initARAP();
            }
        }
    }

    void ARAPManipulator::toggleMode() {
        std::cout << "Move mode set to: " << this->moveMode << std::endl;
        this->initializeSelection();
        this->moveMode = !this->moveMode;
        if(!this->moveMode) {
            for(int i = 0; i < this->manipulators.size(); ++i) {
                this->manipulators[i].lockPosition();
            }
        } else {
            for(int i = 0; i < this->manipulators.size(); ++i) {
                this->manipulators[i].setCustomConstraint();
            }
        }
    }

    void ARAPManipulator::setLockAllManipulators(bool lock) {
        if(lock) {
            for(int i = 0; i < this->manipulators.size(); ++i) {
                this->manipulators[i].lockPosition();
            }
        } else {
            for(int i = 0; i < this->manipulators.size(); ++i) {
                this->manipulators[i].setCustomConstraint();
            }
        }
    }

    void ARAPManipulator::keyPressed(QKeyEvent* e) {
        if(e->key() == Qt::Key_S && !e->isAutoRepeat()) {
            this->toggleMode();
            Q_EMIT needPushHandleButton();
        }
    }

    void ARAPManipulator::keyReleased(QKeyEvent* e) {}

    void ARAPManipulator::mousePressed(QMouseEvent* e) {
        if(e->buttons() == Qt::LeftButton) {
            this->addMode = true;
        }

        if(e->buttons() == Qt::RightButton) {
            this->addMode = false;
        }
    }

    void ARAPManipulator::mouseReleased(QMouseEvent* e) {}

    ARAPManipulator::~ARAPManipulator() {
        ARAPMethod * deformer = dynamic_cast<ARAPMethod*>(this->mesh->meshDeformer);
        if(deformer) {
            deformer->deselectAllPts();
            for(int i = 0; i < this->manipulators.size(); ++i)
                deformer->unsetHandle(i);
        }
        delete this->kid_manip;
    }
/***/

SliceManipulator::SliceManipulator(BaseMesh * mesh, const std::vector<glm::vec3>& positions): MeshManipulator(mesh) {
    this->kid_manip = new RotationManipulator();
    QObject::connect(this->kid_manip, &RotationManipulator::moved, this, [this]() {this->moveKidManip();});
    this->manipulators.reserve(positions.size());
	for (int i = 0; i < positions.size(); ++i) {
		this->manipulators.push_back(Manipulator(positions[i]));
        QObject::connect(&(this->manipulators[i]), &Manipulator::enterAtRangeForGrab, this, &SliceManipulator::displayManipulator);
        QObject::connect(&(this->manipulators[i]), &Manipulator::exitFromRangeForGrab, this, &SliceManipulator::hideManipulator);

        QObject::connect(&(this->manipulators[i]), &Manipulator::mouseRightButtonPressed, this, &SliceManipulator::selectManipulator);
        //QObject::connect(&(this->manipulators[i]), &Manipulator::mouseRightButtonReleasedAndCtrlIsNotPressed, this, &SliceManipulator::deselectManipulator);
        QObject::connect(&(this->manipulators[i]), &Manipulator::isManipulated, this, &SliceManipulator::moveManipulator);

        this->selectedManipulators.push_back(false);
        this->manipulatorsToDisplay.push_back(true);
        this->handles.push_back(false);
		this->manipulators[i].setCustomConstraint();
        this->manipulators[i].enable();
	}

    QObject::connect(&this->selection, &Selection::isSelecting, this, &SliceManipulator::checkSelectedManipulators);

    this->currentSelectedSlice = SliceOrientation::X;
    this->slicesPositions = glm::vec3(0., 0., 0.);
    this->selectSlice(this->currentSelectedSlice);
}

void SliceManipulator::checkSelectedManipulators() {
    this->deselectManipulator(nullptr);
    std::vector<glm::vec3> positions;
    this->getAllPositions(positions);
    std::vector<bool> inSelection = this->selection.areInSelection(positions);
    for(int i = 0; i < inSelection.size(); ++i) {
        if(inSelection[i]) {
            this->selectManipulator(&this->manipulators[i]);
        }
    }
}

void SliceManipulator::setAllManipulatorsPosition(const std::vector<glm::vec3>& positions) {
	if (positions.size() == this->manipulators.size()) {
		for (int i = 0; i < this->manipulators.size(); ++i) {
			this->manipulators[i].setManipPosition(positions[i]);
			this->manipulators[i].setLastPosition(positions[i]);
		}
	} else {
		std::cerr << "WARNING: try to set [" << this->manipulators.size() << "] manipulators positions with a position vector of size [" << positions.size() << "]" << std::endl;
	}
}

void SliceManipulator::getAllPositions(std::vector<glm::vec3>& positions) {
	for (int i = 0; i < this->manipulators.size(); ++i) {
        positions.push_back(this->manipulators[i].getManipPosition());
	}
}

void SliceManipulator::getManipulatorsToDisplay(std::vector<bool>& toDisplay) const {
	for (int i = 0; i < this->manipulatorsToDisplay.size(); ++i) {
        toDisplay.push_back(this->manipulatorsToDisplay[i]);
    }
}

void SliceManipulator::getManipulatorsState(std::vector<State>& states) const {
    states.clear();
    for(int i = 0; i < this->manipulatorsToDisplay.size(); ++i) {
        State currentState = State::NONE;
        if(this->manipulators[i].isAtRangeForGrab)
            currentState = State::AT_RANGE;
        if(this->selectedManipulators[i])
            currentState = State::AT_RANGE;
        if(this->manipulators[i].isSelected)
            currentState = State::MOVE;
        if(this->handles[i])
            currentState = State::LOCK;
        states.push_back(currentState);
    }
}

void SliceManipulator::displayManipulator(Manipulator * manipulatorToDisplay) {
    // Since vectors are organized sequentially, you can get an index by subtracting pointer to initial element 
    // from the pointer to the element you search
    ptrdiff_t index = manipulatorToDisplay - &(this->manipulators[0]);
    this->manipulatorsToDisplay[index] = true;
}

void SliceManipulator::hideManipulator(Manipulator * manipulatorToDisplay) {
    if(manipulatorToDisplay->isSelected)
        return;
    // Since vectors are organized sequentially, you can get an index by subtracting pointer to initial element 
    // from the pointer to the element you search
    ptrdiff_t index = manipulatorToDisplay - &(this->manipulators[0]);
    this->manipulatorsToDisplay[index] = false;
}

void SliceManipulator::moveManipulator(Manipulator * manipulator) {
    this->mesh->movePoint(manipulator->lastPosition, manipulator->getManipPosition());
    Q_EMIT needSendTetmeshToGPU();
}

void SliceManipulator::selectManipulator(Manipulator * manipulator) {
    ptrdiff_t index = manipulator - &(this->manipulators[0]);
    if(!this->selectedManipulators[index]) {
        this->mesh->selectPts(manipulator->getManipPosition());
        this->selectedManipulators[index] = true;
    }
}

void SliceManipulator::deselectManipulator(Manipulator * manipulator) {
    this->deselectAllManipulators(true);
}

void SliceManipulator::deselectAllManipulators(bool keepHandles) {
    for(int i = 0; i < this->selectedManipulators.size(); ++i) {
        if(this->selectedManipulators[i]) {
            this->mesh->deselectPts(this->manipulators[i].getManipPosition());
        }
        this->selectedManipulators[i] = false;
        if(keepHandles && this->handles[i]) {
            ARAPMethod * deformer = dynamic_cast<ARAPMethod*>(this->mesh->meshDeformer);
            if(deformer) {
                deformer->setHandle(i);
            }
        }
    }
}

void SliceManipulator::keyPressed(QKeyEvent* e) {};

void SliceManipulator::keyReleased(QKeyEvent* e) {};

void SliceManipulator::mousePressed(QMouseEvent*) {};

void SliceManipulator::mouseReleased(QMouseEvent*) {};

void SliceManipulator::movePlanes(const glm::vec3& planesPosition) {
    this->deselectAllManipulators(true);
    this->slicesPositions = planesPosition;
    this->selectSlice(this->currentSelectedSlice);
}

void SliceManipulator::selectSlice(SliceOrientation sliceOrientation) {
    int valueIdxToCheck = 0;
    switch(sliceOrientation) {
        case SliceOrientation::X:
            valueIdxToCheck = 0;
            break;
        case SliceOrientation::Y:
            valueIdxToCheck = 1;
            break;
        case SliceOrientation::Z:
            valueIdxToCheck = 2;
            break;
    };

    glm::vec3 mean = glm::vec3(0., 0., 0.);
    float nbSelectedPts = 0;
    glm::vec3 selectionMin = glm::vec3(10000000, 10000000, 10000000);
    glm::vec3 selectionMax = glm::vec3(-10000000, -10000000, -10000000);
    for(int i = 0; i < this->manipulators.size(); ++i) {
        if(std::abs(this->manipulators[i].getManipPosition()[valueIdxToCheck] - this->slicesPositions[valueIdxToCheck]) < this->selectionRadius) {
            this->selectManipulator(&this->manipulators[i]);

            glm::vec3 glmManipPos = this->manipulators[i].getManipPosition();
            mean += glmManipPos;
            nbSelectedPts += 1;
            for(int j = 0; j < 3; ++j) {
                if(selectionMin[j] > glmManipPos[j])
                    selectionMin[j] = glmManipPos[j];
                if(selectionMax[j] < glmManipPos[j])
                    selectionMax[j] = glmManipPos[j];
            }
        }
    }

    if(nbSelectedPts > 0) {
        mean /= nbSelectedPts;
        this->kid_manip->reset();
        this->kid_manip->isVisible = true;
        this->kid_manip->setOrigine(qglviewer::Vec(mean[0], mean[1], mean[2]));
        Q_EMIT needChangeKidManipulatorRadius(glm::length(selectionMax - selectionMin));
        int idxInTransf = 0;
        for(int i = 0; i < this->selectedManipulators.size(); ++i) {
            if(this->selectedManipulators[i]) {
                glm::vec3 glmManipPos = this->manipulators[i].getManipPosition();
                this->kid_manip->addPoint(idxInTransf, qglviewer::Vec(glmManipPos[0], glmManipPos[1], glmManipPos[2]));
                idxInTransf += 1;
            }
        }
    } else {
        this->kid_manip->setOrigine(qglviewer::Vec(-1000., -1000., -1000.));
        this->kid_manip->isVisible = false;
    }
}

void SliceManipulator::updateSliceToSelect(SliceOrientation sliceOrientation) {
    this->deselectAllManipulators(true);
    this->currentSelectedSlice = sliceOrientation;
    this->selectSlice(this->currentSelectedSlice);
}

void SliceManipulator::assignAsHandle() {
    ARAPMethod * deformer = dynamic_cast<ARAPMethod*>(this->mesh->meshDeformer);
    if(!deformer) {
        std::cout << "WARNING: can't assign handles if the deformer isn't ARAP." << std::endl;
        return;
    }
    for(int i = 0; i < this->selectedManipulators.size(); ++i) {
        if(this->selectedManipulators[i]) {
            this->handles[i] = true;
            deformer->setHandle(i);
        }
    }
}

void SliceManipulator::assignAllHandlesBeforePlane() {
    int valueIdxToCheck = 0;
    switch(this->currentSelectedSlice) {
        case SliceOrientation::X:
            valueIdxToCheck = 0;
            break;
        case SliceOrientation::Y:
            valueIdxToCheck = 1;
            break;
        case SliceOrientation::Z:
            valueIdxToCheck = 2;
            break;
    };

    ARAPMethod * deformer = dynamic_cast<ARAPMethod*>(this->mesh->meshDeformer);
    if(!deformer) {
        std::cout << "WARNING: can't assign handles if the deformer isn't ARAP." << std::endl;
        return;
    }

    for(int i = 0; i < this->manipulators.size(); ++i) {
        if(this->manipulators[i].getManipPosition()[valueIdxToCheck] > this->slicesPositions[valueIdxToCheck]) {
            this->handles[i] = true;
            deformer->setHandle(i);
        }
    }
}

void SliceManipulator::removeAllHandles() {
    ARAPMethod * deformer = dynamic_cast<ARAPMethod*>(this->mesh->meshDeformer);
    if(!deformer) {
        std::cout << "WARNING: can't assign handles if the deformer isn't ARAP." << std::endl;
        return;
    }
    for(int i = 0; i < this->handles.size(); ++i) {
        this->handles[i] = false;
        deformer->unsetHandle(i);
    }
}

void SliceManipulator::moveKidManip() {
    std::vector<glm::vec3> originalPoints;
    std::vector<glm::vec3> targetPoints;
    int idxInTransf = 0;
    for(int i = 0; i < this->selectedManipulators.size(); ++i) {
        if(this->selectedManipulators[i]) {
            qglviewer::Vec deformedPoint;
            int trueIndex;
            this->kid_manip->getTransformedPoint(idxInTransf, trueIndex, deformedPoint);
            idxInTransf += 1;
            if(!std::isnan(deformedPoint[0]) && !std::isnan(deformedPoint[1]) && !std::isnan(deformedPoint[2])) {
                originalPoints.push_back(this->manipulators[i].getManipPosition());
                targetPoints.push_back(glm::vec3(deformedPoint[0], deformedPoint[1], deformedPoint[2]));
            }
        }
    }
    this->mesh->movePoints(originalPoints, targetPoints);
    Q_EMIT needSendTetmeshToGPU();
}

/***/

FixedRegistrationManipulator::FixedRegistrationManipulator(BaseMesh * mesh, Grid * gridToRegister, const std::vector<glm::vec3>& positions): MeshManipulator(mesh), gridToRegister(gridToRegister) {
    this->kid_manip = nullptr;
    this->manipulators.reserve(positions.size());
    this->toolState = FixedRegistrationManipulatorState::NONE;
    this->selectedIndex = -1;
    this->fixed = std::vector<int>{206, 36, 17, 97, 39, 68, 106, 58, 74};
    this->nbNotAssociatedPoints = fixed.size();
    for (int i = 0; i < fixed.size(); ++i) {
        this->manipulators.push_back(Manipulator(positions[fixed[i]]));

        QObject::connect(&(this->manipulators[i]), &Manipulator::mouseRightButtonPressed, this, &FixedRegistrationManipulator::selectManipulator);
        QObject::connect(&(this->manipulators[i]), &Manipulator::mouseRightButtonReleasedAndCtrlIsNotPressed, this, &FixedRegistrationManipulator::deselectManipulator);
        QObject::connect(&(this->manipulators[i]), &Manipulator::isManipulated, this, &FixedRegistrationManipulator::moveManipulator);
        QObject::connect(this, &FixedRegistrationManipulator::pointIsClickedInPlanarViewer, this, &FixedRegistrationManipulator::addManipulator);

        QObject::connect(this, &FixedRegistrationManipulator::rayIsCasted, this, &FixedRegistrationManipulator::addManipulatorFromRay);

        this->associatedManipulator.push_back(-1);
        this->isFixed.push_back(true);
        this->selectedManipulators.push_back(false);
        this->manipulatorsToDisplay.push_back(true);
        this->manipulators[i].lockPosition();
        this->manipulators[i].enable();
    }
}

void FixedRegistrationManipulator::addManipulator(const glm::vec3& position) {
    if(this->toolState == FixedRegistrationManipulatorState::SELECTING_SECOND_POINT) {
        this->toolState = FixedRegistrationManipulatorState::NONE;

        this->manipulators.push_back(Manipulator(position));
        this->isFixed.push_back(false);
        this->manipulatorsToDisplay.push_back(true);
        this->manipulators.back().lockPosition();
        this->manipulators.back().disable();

        if(this->associatedManipulator[this->selectedIndex] == -1)
            this->nbNotAssociatedPoints -= 1;
        this->associatedManipulator[this->selectedIndex] = this->manipulators.size()-1;
        this->selectedManipulators[this->selectedIndex] = false;
        std::cout << "Associate vertex [" << selectedIndex << "] with [" << this->manipulators.back().getManipPosition() << "]" << std::endl;
    }
}

void FixedRegistrationManipulator::addManipulatorFromRay(const glm::vec3& origin, const glm::vec3& direction, uint16_t minValue, uint16_t maxValue, const glm::vec3& planePos) {
    glm::vec3 manipulatorPosition;
    if(this->gridToRegister->getPositionOfRayIntersection(origin, direction, minValue, maxValue, planePos, manipulatorPosition))
        this->addManipulator(manipulatorPosition);
}

void FixedRegistrationManipulator::setAllManipulatorsPosition(const std::vector<glm::vec3>& positions) {
    if (positions.size() == this->manipulators.size()) {
        for (int i = 0; i < this->manipulators.size(); ++i) {
            this->manipulators[i].setManipPosition(positions[i]);
            this->manipulators[i].setLastPosition(positions[i]);
        }
    } else {
        std::cerr << "WARNING: try to set [" << this->manipulators.size() << "] manipulators positions with a position vector of size [" << positions.size() << "]" << std::endl;
    }
}

void FixedRegistrationManipulator::getAllPositions(std::vector<glm::vec3>& positions) {
    for (int i = 0; i < this->manipulators.size(); ++i) {
        positions.push_back(this->manipulators[i].getManipPosition());
    }
}

void FixedRegistrationManipulator::getManipulatorsToDisplay(std::vector<bool>& toDisplay) const {
    for (int i = 0; i < this->manipulatorsToDisplay.size(); ++i) {
        toDisplay.push_back(this->manipulatorsToDisplay[i]);
    }
}

void FixedRegistrationManipulator::getManipulatorsState(std::vector<State>& states) const {
    states.clear();
    for(int i = 0; i < this->manipulatorsToDisplay.size(); ++i) {
        State currentState = State::NONE;
        if(this->isFixed[i]) {
            if(this->manipulators[i].isAtRangeForGrab)
                currentState = State::AT_RANGE;
            if(this->selectedManipulators[i])
                currentState = State::AT_RANGE;
            if(this->associatedManipulator[i] != -1)
                currentState = State::HIGHLIGHT;
        } else {
            currentState = State::LOCK;
        }
        states.push_back(currentState);
    }
}

void FixedRegistrationManipulator::moveManipulator(Manipulator * manipulator) {
}

void FixedRegistrationManipulator::selectManipulator(Manipulator * manipulator) {
    ptrdiff_t index = manipulator - &(this->manipulators[0]);
    if(this->toolState == FixedRegistrationManipulatorState::NONE && this->isFixed[index]) {
        if(this->associatedManipulator[index] != -1) {
            std::cout << "Replace associated point" << std::endl;
            int previousIdx = this->associatedManipulator[index];
            std::cout << "Previous: " << previousIdx << std::endl;
            this->associatedManipulator[index] = -1;
            this->manipulators[previousIdx].lockPosition();
            this->manipulators[previousIdx].disable();
            this->manipulatorsToDisplay[previousIdx] = false;
            this->isFixed[previousIdx] = false;
        }
        this->selectedManipulators[index] = true;
        this->toolState = FixedRegistrationManipulatorState::SELECTING_SECOND_POINT;
        this->selectedIndex = index;
        std::cout << "Select vertex: [" << index << "]" << std::endl;
    }
}

void FixedRegistrationManipulator::apply() {
    std::vector<int> verticesToFit;
    std::vector<glm::vec3> newPositions;
    std::cout << "Selected pairs are:" << std::endl;
    for(int i = 0; i < this->associatedManipulator.size(); ++i) {
        if(this->associatedManipulator[i] >= 0) {
            verticesToFit.push_back(this->fixed[i]);
            newPositions.push_back(this->manipulators[this->associatedManipulator[i]].getManipPosition());
        }
    }
    this->mesh->setARAPDeformationMethod();
    ARAPMethod * deformer = dynamic_cast<ARAPMethod*>(this->mesh->meshDeformer);
    if(deformer) {
        deformer->fitToPointList(verticesToFit, newPositions);
    }

    for(int i = 0; i < this->fixed.size(); ++i) {
        this->manipulators[i].setManipPosition(this->mesh->vertices[this->fixed[i]]);
    }
}

void FixedRegistrationManipulator::deselectManipulator(Manipulator * manipulator) {}

void FixedRegistrationManipulator::keyPressed(QKeyEvent* e) {};

void FixedRegistrationManipulator::keyReleased(QKeyEvent* e) {};

void FixedRegistrationManipulator::mousePressed(QMouseEvent*) {};

void FixedRegistrationManipulator::mouseReleased(QMouseEvent*) {};

}

