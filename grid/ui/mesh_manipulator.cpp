#include "mesh_manipulator.hpp"
#include "manipulator.hpp"


namespace UITool {
	DirectManipulator::DirectManipulator(BaseMesh * mesh, const std::vector<glm::vec3>& positions): MeshManipulator(mesh) {
		this->active = false;
        this->manipulators.reserve(positions.size());
		for (int i = 0; i < positions.size(); ++i) {
			this->manipulators.push_back(Manipulator(positions[i]));
            QObject::connect(&(this->manipulators[i]), &Manipulator::enterAtRangeForGrab, this, &DirectManipulator::displayManipulator);
            QObject::connect(&(this->manipulators[i]), &Manipulator::exitFromRangeForGrab, this, &DirectManipulator::hideManipulator);

            QObject::connect(&(this->manipulators[i]), &Manipulator::mouseRightButtonPressed, this, &DirectManipulator::selectManipulator);
            QObject::connect(&(this->manipulators[i]), &Manipulator::mouseRightButtonReleasedAndCtrlIsNotPressed, this, &DirectManipulator::deselectManipulator);
            QObject::connect(&(this->manipulators[i]), &Manipulator::isManipulated, this, &DirectManipulator::moveManipulator);

            this->manipulatorsToDisplay.push_back(false);
			this->manipulators[i].lockPosition();
            this->manipulators[i].disable();
		}
	}

	void DirectManipulator::setActivation(bool isActive) {
        this->active = isActive;
		if (this->active) {
			for (int i = 0; i < this->manipulators.size(); ++i) {
				this->manipulators[i].setCustomConstraint();
                this->manipulatorsToDisplay[i] = false;
                this->manipulators[i].enable();
			}
		} else {
			for (int i = 0; i < this->manipulators.size(); ++i) {
				this->manipulators[i].lockPosition();
                this->manipulatorsToDisplay[i] = false;
                this->manipulators[i].disable();
			}
		}
	}

    void DirectManipulator::addManipulator(const glm::vec3& position) {
        // Nothing to do here
        // We deactivate the add manipulator functionnality because we need a MeshDeformator to create a manipulator
        // And its hard to get from here
        //this->manipulators.push_back(Manipulator(position));
        //if(this->active) {
        //    this->manipulatorsToDisplay.push_back(true);
        //} else {
        //    this->manipulatorsToDisplay.push_back(false);
        //}
    }

    void DirectManipulator::removeManipulator(Manipulator * manipulatorToDisplay) {
        //this->manipulators.erase(this->manipulators.begin()+idx);
        //this->manipulatorsToDisplay.erase(this->manipulatorsToDisplay.begin()+idx);
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
            if(this->manipulators[i].isSelected)
                currentState = State::MOVE;
            states.push_back(currentState);
        }
    }

    void DirectManipulator::displayManipulator(Manipulator * manipulatorToDisplay) {
        if(!this->active)
            return;
        // Since vectors are organized sequentially, you can get an index by subtracting pointer to initial element 
        // from the pointer to the element you search
        ptrdiff_t index = manipulatorToDisplay - &(this->manipulators[0]);
        this->manipulatorsToDisplay[index] = true;
    }

    void DirectManipulator::hideManipulator(Manipulator * manipulatorToDisplay) {
        if(!this->active || manipulatorToDisplay->isSelected)
            return;
        // Since vectors are organized sequentially, you can get an index by subtracting pointer to initial element 
        // from the pointer to the element you search
        ptrdiff_t index = manipulatorToDisplay - &(this->manipulators[0]);
        this->manipulatorsToDisplay[index] = false;
    }

    bool DirectManipulator::isWireframeDisplayed() {
        return this->active;
    }

    void DirectManipulator::moveManipulator(Manipulator * manipulator) {
        this->mesh->movePoint(manipulator->lastPosition, manipulator->getManipPosition());
        Q_EMIT needSendTetmeshToGPU();
    }

    void DirectManipulator::selectManipulator(Manipulator * manipulator) {
        this->mesh->selectPts(manipulator->getManipPosition());
    }

    void DirectManipulator::deselectManipulator(Manipulator * manipulator) {
        this->mesh->deselectAllPts();
        this->hideManipulator(manipulator);
    }

    /***/

	FreeManipulator::FreeManipulator(BaseMesh * mesh, const std::vector<glm::vec3>& positions): MeshManipulator(mesh), manipulator(Manipulator(glm::vec3(0., 0., 0.))) {
		this->active = false;
        QObject::connect(&(this->manipulator), &Manipulator::mouseRightButtonPressed, this, &FreeManipulator::selectManipulator);
        QObject::connect(&(this->manipulator), &Manipulator::mouseRightButtonReleasedAndCtrlIsNotPressed, this, &FreeManipulator::deselectManipulator);
        QObject::connect(&(this->manipulator), &Manipulator::isManipulated, this, &FreeManipulator::moveManipulator);

        QObject::connect(this, &FreeManipulator::keyQReleased, this, [this]{setActivation(false);});
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

    void FreeManipulator::removeManipulator(Manipulator * manipulatorToDisplay) {
        this->setActivation(false);
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

    void FreeManipulator::displayManipulator(Manipulator * manipulatorToDisplay) {
        //Do nothin because display is directly link to active variable
        return;
    }

    void FreeManipulator::hideManipulator(Manipulator * manipulatorToDisplay) {
        //Do nothin because display is directly link to active variable
        return;
    }

    bool FreeManipulator::isWireframeDisplayed() {
        return false;
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
        this->removeManipulator(manipulator);
    }

    void FreeManipulator::addManipulatorFromRay(const glm::vec3& origin, const glm::vec3& direction, uint16_t minValue, uint16_t maxValue) {
        glm::vec3 manipulatorPosition;
        if(this->mesh->getPositionOfRayIntersection(origin, direction, minValue, maxValue, manipulatorPosition))
            this->addManipulator(manipulatorPosition);
    }

    /***/

	PositionManipulator::PositionManipulator(BaseMesh * mesh, const std::vector<glm::vec3>& positions): MeshManipulator(mesh), manipulator(Manipulator(glm::vec3(0., 0., 0.))) {
		this->active = false;
        QObject::connect(&(this->manipulator), &Manipulator::mouseRightButtonPressed, this, &PositionManipulator::selectManipulator);
        QObject::connect(&(this->manipulator), &Manipulator::mouseRightButtonReleasedAndCtrlIsNotPressed, this, &PositionManipulator::deselectManipulator);
        QObject::connect(&(this->manipulator), &Manipulator::isManipulated, this, &PositionManipulator::moveManipulator);

        this->manipulator.setCustomConstraint();
        this->manipulator.setManipPosition(mesh->getOrigin());
	}

    void PositionManipulator::setActivation(bool isActive) {
        this->active = isActive;
        if (this->active) {
            this->manipulator.setCustomConstraint();
        } else {
            this->manipulator.lockPosition();
        }
    }

    void PositionManipulator::addManipulator(const glm::vec3& position) {
    }

    void PositionManipulator::removeManipulator(Manipulator * manipulatorToDisplay) {
    }

	void PositionManipulator::setAllManipulatorsPosition(const std::vector<glm::vec3>& positions) {
        //Do nothing, because we don't want to move these manipulator
	}

    void PositionManipulator::getAllPositions(std::vector<glm::vec3>& positions) {
        positions.push_back(this->manipulator.getManipPosition());
    }

    void PositionManipulator::getManipulatorsToDisplay(std::vector<bool>& toDisplay) const {
        toDisplay.push_back(this->active);
    }

    void PositionManipulator::getManipulatorsState(std::vector<State>& states) const {
        states.clear();
        State currentState = State::NONE;
        if(this->manipulator.isAtRangeForGrab)
            currentState = State::AT_RANGE;
        if(this->manipulator.isSelected)
            currentState = State::MOVE;
        states.push_back(currentState);
    }

    void PositionManipulator::displayManipulator(Manipulator * manipulatorToDisplay) {
        //Do nothin because display is directly link to active variable
        return;
    }

    void PositionManipulator::hideManipulator(Manipulator * manipulatorToDisplay) {
        //Do nothin because display is directly link to active variable
        return;
    }

    bool PositionManipulator::isWireframeDisplayed() {
        return false;
    }

    void PositionManipulator::moveManipulator(Manipulator * manipulator) {
        //this->mesh->translate(manipulator->getManipPosition() - manipulator->lastPosition);
        this->mesh->setOrigin(manipulator->getManipPosition());
    }

    void PositionManipulator::selectManipulator(Manipulator * manipulator) {
    }

    void PositionManipulator::deselectManipulator(Manipulator * manipulator) {
    }
}
