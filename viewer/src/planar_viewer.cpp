#include "../include/planar_viewer.hpp"

#include <QCoreApplication>
#include <QProgressDialog>
#include <QMouseEvent>
#include <QGuiApplication>

PlanarViewer::PlanarViewer(Scene* const _scene, planes _p, planeHeading _h, QWidget* parent) :
		QGLViewer(parent), sceneToShow(_scene), planeToShow(_p), planeOrientation(_h) {
	this->setGridIsDrawn(false);
	this->setAxisIsDrawn(false);
	this->setCameraIsEdited(false);

	this->viewerController = nullptr;

	this->minZoomRatio = .1f;
	this->zoomRatio = 1.f;
	this->maxZoomRatio = 500.f;
	this->offset = glm::vec2(0.f, 0.f);

	this->refreshTimer = new QTimer();
	// ~7 ms for 144fps, ~16ms for 60fps and ~33ms for 30 FPS
	this->refreshTimer->setInterval(std::chrono::milliseconds(500)); // 1/2 second when not updated by the viewer
	this->refreshTimer->setSingleShot(false);
	connect(this->refreshTimer, &QTimer::timeout, this, &PlanarViewer::updateView);
}

PlanarViewer::~PlanarViewer(void) {
	// Nothing here yet.
	if (this->viewerController != nullptr) {
		this->viewerController->unregisterPlaneViewer();
	}
}

void PlanarViewer::init(void) {
	if (this->sceneToShow == nullptr) {
		throw std::runtime_error("[ERROR] Scene was nullptr when initialized.");
	}

	this->makeCurrent();

	this->sceneToShow->initGl(this->context());

	this->refreshTimer->start();
}

void PlanarViewer::draw(void) {
	glClearColor(.8, .8, .8, 1.);

	QSize viewerSize = this->size();
	glm::vec2 fbDims = glm::vec2(static_cast<float>(viewerSize.width()), static_cast<float>(viewerSize.height()));

	this->sceneToShow->drawPlaneView(fbDims, this->planeToShow, this->planeOrientation, this->zoomRatio, this->offset);
}

void PlanarViewer::keyPressEvent(QKeyEvent* _e) {
	switch (_e->key()) {
		/*
		SHADER PROGRAMS
		*/
		case Qt::Key::Key_F5:
			this->sceneToShow->recompileShaders();
			this->update();
		break;
		case Qt::Key::Key_P:
			this->sceneToShow->printVAOStateNext();
			this->update();
		break;
		/*
		MOUSE MOVEMENT
		*/
		case Qt::Key::Key_R:
			if (not this->mouse_isPressed) {
				this->offset = glm::vec2(.0, .0);
				this->zoomRatio = 1.f;
			}
			this->update();
		break;
		default:
			QGLViewer::keyPressEvent(_e);
			this->update();
		break;
	}
}

void PlanarViewer::mousePressEvent(QMouseEvent* _e) {
	if (_e->buttons().testFlag(Qt::MouseButton::RightButton)) {
		this->mouse_isPressed = true;
		this->lastPosition = _e->pos();
	}
	QGLViewer::mousePressEvent(_e);
	this->update();
}

void PlanarViewer::mouseMoveEvent(QMouseEvent* _m) {
	if (this->mouse_isPressed) {
		QPoint currentPos = _m->pos();
		QSize viewerSize = this->size();
		QPoint viewerPos = this->pos();
		glm::vec2 minViewer = glm::vec2(static_cast<float>(viewerPos.x()), static_cast<float>(viewerPos.y()));
		glm::vec2 maxViewer = minViewer + glm::vec2(static_cast<float>(viewerSize.width()), static_cast<float>(viewerSize.height()));
		// absolute positions of the mouse in last pos and current pos :
		glm::vec2 absPosMouse = glm::vec2(static_cast<float>(currentPos.x()), static_cast<float>(currentPos.y()));
		glm::vec2 absPosLastPos = glm::vec2(static_cast<float>(this->lastPosition.x()), static_cast<float>(this->lastPosition.y()));
		this->offset += (absPosMouse - absPosLastPos) / (maxViewer - minViewer);
		this->lastPosition = currentPos;
	} else {
	}
	QGLViewer::mouseMoveEvent(_m);
	return;
}

void PlanarViewer::mouseReleaseEvent(QMouseEvent* _m) {
	//
	if (_m->button() == Qt::MouseButton::RightButton) {
		this->mouse_isPressed = false;
		this->lastPosition = _m->pos();
	}
	QGLViewer::mouseReleaseEvent(_m);
	this->update();
}

void PlanarViewer::wheelEvent(QWheelEvent* _w) {
	if (_w->angleDelta().y() > 0) {
		this->zoomRatio *= 1.1;
	} else {
		this->zoomRatio *= .9;
	}

	if (this->zoomRatio > this->maxZoomRatio) { this->zoomRatio = this->maxZoomRatio; }
	if (this->zoomRatio < this->minZoomRatio) { this->zoomRatio = this->minZoomRatio; }

	QGLViewer::wheelEvent(_w);
	this->update();
}

void PlanarViewer::setController(ViewerHeader* _header) {
	this->viewerController = _header;
}

void PlanarViewer::updateView() {
	// Done because for some reason we coundn't connect the signal
	// from the timer's timeout to the update slot directly. Dumb.
	this->update();
}

void PlanarViewer::updatePlaneDepth(int newVal) {
	float scalar = static_cast<float>(newVal) / 100.f;
	if (this->planeToShow == planes::x) { this->sceneToShow->slotSetPlaneDisplacementX(scalar); }
	if (this->planeToShow == planes::y) { this->sceneToShow->slotSetPlaneDisplacementY(scalar); }
	if (this->planeToShow == planes::z) { this->sceneToShow->slotSetPlaneDisplacementZ(scalar); }
	this->update();
}

void PlanarViewer::flipPlaneDirection() {
	// The plane inversion doesn't happen here (only visible in 3D view) however the buttons to control it
	// are contained in the headers of the planar viewers. Thus, it must be placed here.
	if (this->planeToShow == planes::x) { this->sceneToShow->slotTogglePlaneDirectionX(); }
	if (this->planeToShow == planes::y) { this->sceneToShow->slotTogglePlaneDirectionY(); }
	if (this->planeToShow == planes::z) { this->sceneToShow->slotTogglePlaneDirectionZ(); }
	this->update();
}

void PlanarViewer::rotatePlaneClockwise() {
	if (this->planeOrientation == planeHeading::North) { this->planeOrientation = planeHeading::East; }
	else if (this->planeOrientation == planeHeading::East) { this->planeOrientation = planeHeading::South; }
	else if (this->planeOrientation == planeHeading::South) { this->planeOrientation = planeHeading::West; }
	else if (this->planeOrientation == planeHeading::West) { this->planeOrientation = planeHeading::North; }
	this->sceneToShow->setPlaneHeading(this->planeToShow, this->planeOrientation);
	this->update();
}

void PlanarViewer::rotatePlaneCounterClockwise() {
	if (this->planeOrientation == planeHeading::North) { this->planeOrientation = planeHeading::West; }
	else if (this->planeOrientation == planeHeading::West) { this->planeOrientation = planeHeading::South; }
	else if (this->planeOrientation == planeHeading::South) { this->planeOrientation = planeHeading::East; }
	else if (this->planeOrientation == planeHeading::East) { this->planeOrientation = planeHeading::North; }
	this->sceneToShow->setPlaneHeading(this->planeToShow, this->planeOrientation);
	this->update();
}

void PlanarViewer::togglePlaneVisibility() {
	this->sceneToShow->togglePlaneVisibility(this->planeToShow);
}
