#include "../include/neighbor_visu_main_widget.hpp"

#include <QLabel>
#include <QEvent>
#include <QSplitter>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSizePolicy>

MainWidget::MainWidget() {
	this->strayObj.clear();
	this->setupWidgets();
	this->widgetSizeSet = false;
}

MainWidget::~MainWidget() {
	this->removeEventFilter(this);
	this->xPlaneDepth->disconnect();
	this->yPlaneDepth->disconnect();
	this->zPlaneDepth->disconnect();
	for (std::size_t i = 0; i < this->strayObj.size(); ++i) {
		if (this->strayObj[i] != nullptr) {
			delete this->strayObj[i];
			this->strayObj[i] = nullptr;
		}
	}
	delete this->viewer_planeX;
	delete this->viewer_planeY;
	delete this->viewer_planeZ;
	delete this->scene;
	delete this->gridController;
	delete this->controlPanel;
	this->strayObj.clear();
}

void MainWidget::setupWidgets() {
	this->gridController = new GridControl(nullptr);
	this->scene = new Scene(this->gridController);

	this->viewer = new Viewer(this->scene, nullptr);
	this->controlPanel = new ControlPanel(this->scene, this->viewer, nullptr, nullptr);
	this->scene->setControlPanel(this->controlPanel);

	this->viewer_planeX = new PlanarViewer(this->scene, planes::x, nullptr);
	this->viewer_planeY = new PlanarViewer(this->scene, planes::y, nullptr);
	this->viewer_planeZ = new PlanarViewer(this->scene, planes::z, nullptr);

	// Sliders for each plane (also sets range and values) :
	this->xPlaneDepth = new QSlider(Qt::Horizontal);
	this->yPlaneDepth = new QSlider(Qt::Horizontal);
	this->zPlaneDepth = new QSlider(Qt::Horizontal);
	this->xPlaneDepth->setRange(0, 100); this->xPlaneDepth->setValue(0);
	this->yPlaneDepth->setRange(0, 100); this->yPlaneDepth->setValue(0);
	this->zPlaneDepth->setRange(0, 100); this->zPlaneDepth->setValue(0);

	// The signals for plane depth will be connected :
	connect(this->xPlaneDepth, &QSlider::valueChanged, this, &MainWidget::setXTexCoord);
	connect(this->yPlaneDepth, &QSlider::valueChanged, this, &MainWidget::setYTexCoord);
	connect(this->zPlaneDepth, &QSlider::valueChanged, this, &MainWidget::setZTexCoord);

	// Splitters : one main (hor.) and two secondaries (vert.) :
	QSplitter* mainSplit = new QSplitter(Qt::Horizontal);
	QSplitter* splitAbove = new QSplitter(Qt::Vertical);
	QSplitter* splitAbove1 = new QSplitter(Qt::Vertical);

	// Layouts to place a viewer and a slider in the same place :
	QVBoxLayout* vPX = new QVBoxLayout();
	QVBoxLayout* vPY = new QVBoxLayout();
	QVBoxLayout* vPZ = new QVBoxLayout();

	// Those will encapsulate the layouts above :
	QWidget* xViewerCapsule = new QWidget();
	QWidget* yViewerCapsule = new QWidget();
	QWidget* zViewerCapsule = new QWidget();

	// Add the sub-splits to the main one :
	mainSplit->addWidget(splitAbove);
	mainSplit->addWidget(splitAbove1);

	// Set the layouts for the plane viewers :
	vPX->addWidget(this->viewer_planeX);
	vPX->addWidget(this->xPlaneDepth);
	vPY->addWidget(this->viewer_planeY);
	vPY->addWidget(this->yPlaneDepth);
	vPZ->addWidget(this->viewer_planeZ);
	vPZ->addWidget(this->zPlaneDepth);

	// Encapsulate the layouts above :
	xViewerCapsule->setLayout(vPX);
	yViewerCapsule->setLayout(vPY);
	zViewerCapsule->setLayout(vPZ);

	// Add to splits in order to show them all :
	splitAbove->addWidget(this->viewer);
	splitAbove->addWidget(xViewerCapsule);
	splitAbove1->addWidget(yViewerCapsule);
	splitAbove1->addWidget(zViewerCapsule);

	QHBoxLayout* viewerLayout = new QHBoxLayout();
	viewerLayout->addWidget(mainSplit);

	QVBoxLayout* mainLayout = new QVBoxLayout();
	mainLayout->addLayout(viewerLayout);
	mainLayout->addWidget(this->controlPanel);
	mainLayout->setAlignment(this->controlPanel, Qt::AlignHCenter);
	this->gridController->show(); // Enable grid controller as a floating window

	this->strayObj.push_back(viewerLayout);
	this->strayObj.push_back(mainLayout);
	this->strayObj.push_back(mainSplit);
	this->strayObj.push_back(splitAbove);
	this->strayObj.push_back(splitAbove1);
	this->strayObj.push_back(xViewerCapsule);
	this->strayObj.push_back(yViewerCapsule);
	this->strayObj.push_back(zViewerCapsule);

	this->setLayout(mainLayout);
	this->installEventFilter(this);
}

bool MainWidget::eventFilter(QObject* obj, QEvent* e) {
	// Set our code to run after the original event handling :
	if (this->widgetSizeSet == false && obj == this && e->type() == QEvent::Show) {
		this->widgetSizeSet = true;
		// lock control panel size to the current size it has :
		this->controlPanel->setMinimumSize(this->controlPanel->size());
		this->controlPanel->setMaximumSize(this->controlPanel->size());
		// set the viewer to have a minimum size of controlPanelSize on width/height :
		this->viewer->setMinimumWidth(this->controlPanel->minimumWidth());
		this->viewer->setMinimumHeight(this->controlPanel->minimumWidth()/2);
	}
	// Return false, to handle the rest of the event normally
	return false;
}

void MainWidget::setXTexCoord(int coordX) {
	float max = static_cast<float>(this->xPlaneDepth->maximum());
	float ratio = static_cast<float>(coordX) / max;
	this->scene->slotSetPlaneDepthX(ratio);
	this->viewer->update();
	this->viewer_planeX->update();
	this->viewer_planeY->update();
	this->viewer_planeZ->update();
}

void MainWidget::setYTexCoord(int coordY) {
	float max = static_cast<float>(this->yPlaneDepth->maximum());
	float ratio = static_cast<float>(coordY) / max;
	this->scene->slotSetPlaneDepthY(ratio);
	this->viewer->update();
	this->viewer_planeX->update();
	this->viewer_planeY->update();
	this->viewer_planeZ->update();
}

void MainWidget::setZTexCoord(int coordZ) {
	float max = static_cast<float>(this->zPlaneDepth->maximum());
	float ratio = static_cast<float>(coordZ) / max;
	this->scene->slotSetPlaneDepthZ(ratio);
	this->viewer->update();
	this->viewer_planeX->update();
	this->viewer_planeY->update();
	this->viewer_planeZ->update();
}
