#include "../include/loader_widget.hpp"

#include "../include/user_settings_widget.hpp"

#include <glm/gtx/io.hpp>

#include <QCoreApplication>
#include <QDialog>
#include <QFileDialog>
#include <QMessageBox>

#include <iomanip>

GridLoaderWidget::GridLoaderWidget(Scene* _scene, Viewer* _viewer, ControlPanel* cp, QWidget* parent) :
	QWidget(parent) {

	this->setAttribute(Qt::WA_DeleteOnClose);
	this->basePath.setPath(QDir::homePath());
	this->scene					= _scene;
	this->viewer				= _viewer;
	this->_cp					= cp;

	this->_testing_grid = nullptr;

    this->label_header = new QLabel("<h2><i>Load a grid</i></h2>");

    this->button_loadGrids = new QPushButton("Load grids into program");

    this->layout_subsample;
	this->group_subsample = new QGroupBox("Image subsample at loading");
	this->group_subsample->setToolTip("Allow to open the image and visualize it subsampled.");
	this->group_subsample->setCheckable(true);
	this->group_subsample->setChecked(false);
    this->label_subsample = new QLabel("Subsample");
    this->spinbox_subsample = new QSpinBox;
    this->spinbox_subsample->setValue(2.);
	this->spinbox_subsample->setRange(2., 2000.);
	this->spinbox_subsample->setSingleStep(2.);

    this->layout_bbox;
	this->group_bbox = new QGroupBox("Image subregion");
	this->group_bbox->setToolTip("Allow to open only a subregion of the image.");
	this->group_bbox->setCheckable(true);
	this->group_bbox->setChecked(false);
    this->label_bbox = new QLabel("Image subregion");
    this->label_bboxMin = new QLabel("BBox min");
    this->spinbox_bboxMin_x = new QDoubleSpinBox;
    this->spinbox_bboxMin_x->setValue(0.);
    this->spinbox_bboxMin_x->setRange(0., 1000000.);
    this->spinbox_bboxMin_y = new QDoubleSpinBox;
    this->spinbox_bboxMin_y->setValue(0.);
    this->spinbox_bboxMin_y->setRange(0., 1000000.);
    this->spinbox_bboxMin_z = new QDoubleSpinBox;
    this->spinbox_bboxMin_z->setValue(0.);
    this->spinbox_bboxMin_z->setRange(0., 1000000.);
    this->label_bboxMax = new QLabel("BBox max");
    this->spinbox_bboxMax_x = new QDoubleSpinBox;
    this->spinbox_bboxMax_x->setValue(0.);
    this->spinbox_bboxMax_x->setRange(0., 1000000.);
    this->spinbox_bboxMax_y = new QDoubleSpinBox;
    this->spinbox_bboxMax_y->setValue(0.);
    this->spinbox_bboxMax_y->setRange(0., 1000000.);
    this->spinbox_bboxMax_z = new QDoubleSpinBox;
    this->spinbox_bboxMax_z->setValue(0.);
    this->spinbox_bboxMax_z->setRange(0., 1000000.);

	this->group_mesh = new QGroupBox("Size of the tetrahedral mesh");
	this->group_mesh->setToolTip("Allow to specify the size of the tetrahedral mesh");
	this->group_mesh->setCheckable(false);
	this->label_mesh = new QLabel("Nb tetrahedra");
	this->spinbox_mesh_x = new QDoubleSpinBox;
    this->spinbox_mesh_x->setValue(5.);
    this->spinbox_mesh_x->setRange(0., 1000.);

	this->spinbox_mesh_y = new QDoubleSpinBox;
    this->spinbox_mesh_y->setValue(5.);
    this->spinbox_mesh_y->setRange(0., 1000.);

	this->spinbox_mesh_z = new QDoubleSpinBox;
    this->spinbox_mesh_z->setValue(5.);
    this->spinbox_mesh_z->setRange(0., 1000.);

	this->group_voxel = new QGroupBox("Size of the voxels");
	this->group_voxel->setToolTip("Allow to specify the size of the voxels");
	this->group_voxel->setCheckable(false);
	this->label_voxel = new QLabel("Nb tetrahedra");
	this->spinbox_voxel_x = new QDoubleSpinBox;
    this->spinbox_voxel_x->setValue(1.);
    this->spinbox_voxel_x->setRange(0., 1000.);

	this->spinbox_voxel_y = new QDoubleSpinBox;
    this->spinbox_voxel_y->setValue(1.);
    this->spinbox_voxel_y->setRange(0., 1000.);

	this->spinbox_voxel_z = new QDoubleSpinBox;
    this->spinbox_voxel_z->setValue(1.);
    this->spinbox_voxel_z->setRange(0., 1000.);

    this->button_loadNewGridAPI = new QPushButton("Load with new grid API");

	this->progress_load			   = new QProgressBar;
	QSizePolicy retain_size_policy = this->progress_load->sizePolicy();
	retain_size_policy.setRetainSizeWhenHidden(true);
	this->progress_load->setSizePolicy(retain_size_policy);

    this->setupLayouts();
	this->setupSignals();
}

void GridLoaderWidget::setupLayouts() {

    this->mainLayout = new QVBoxLayout;

	this->layout_subsample = new QHBoxLayout;
	this->layout_bbox = new QGridLayout;
	this->layout_mesh = new QHBoxLayout;
	this->layout_voxel = new QHBoxLayout;

	this->group_bbox->setLayout(this->layout_bbox);
	this->group_subsample->setLayout(this->layout_subsample);
    this->group_mesh->setLayout(this->layout_mesh);
    this->group_voxel->setLayout(this->layout_voxel);

	this->layout_bbox->addWidget(this->label_bboxMin, 0, 0);
	this->layout_bbox->addWidget(this->spinbox_bboxMin_x, 0, 1);
	this->layout_bbox->addWidget(this->spinbox_bboxMin_y, 0, 2);
	this->layout_bbox->addWidget(this->spinbox_bboxMin_z, 0, 3);

	this->layout_bbox->addWidget(this->label_bboxMax, 1, 0);
	this->layout_bbox->addWidget(this->spinbox_bboxMax_x, 1, 1);
	this->layout_bbox->addWidget(this->spinbox_bboxMax_y, 1, 2);
	this->layout_bbox->addWidget(this->spinbox_bboxMax_z, 1, 3);

	this->layout_subsample->addWidget(this->label_subsample);
	this->layout_subsample->addWidget(this->spinbox_subsample);

	this->layout_mesh->addWidget(this->label_mesh);
	this->layout_mesh->addWidget(this->spinbox_mesh_x);
	this->layout_mesh->addWidget(this->spinbox_mesh_y);
	this->layout_mesh->addWidget(this->spinbox_mesh_z);

	//this->layout_voxel->addWidget(this->label_voxel);
	this->layout_voxel->addWidget(this->spinbox_voxel_x);
	this->layout_voxel->addWidget(this->spinbox_voxel_y);
	this->layout_voxel->addWidget(this->spinbox_voxel_z);

	this->progress_load->hide();

    this->mainLayout->addWidget(this->group_subsample);
    this->mainLayout->addWidget(this->group_bbox);
    this->mainLayout->addWidget(this->group_mesh);
    this->mainLayout->addWidget(this->group_voxel);
    //this->mainLayout->addWidget(this->button_loadGrids);
    this->mainLayout->addWidget(this->button_loadNewGridAPI);
    this->setLayout(this->mainLayout);
}

GridLoaderWidget::~GridLoaderWidget() {
	this->scene	 = nullptr;
	this->viewer = nullptr;

	//this->button_loadGrids->disconnect();
	//delete this->groupbox_userLimits;
}

void GridLoaderWidget::setupSignals() {
	if (this->scene == nullptr) {
		QMessageBox* msgBox = new QMessageBox;
		msgBox->setAttribute(Qt::WA_DeleteOnClose);
		msgBox->critical(this, "Error : no scene associated", "An error has occured, and no scene was associated with this widget. Please retry again later.");
		return;
	}

	if (this->viewer == nullptr) {
		QMessageBox* msgBox = new QMessageBox;
		msgBox->setAttribute(Qt::WA_DeleteOnClose);
		msgBox->critical(this, "Error : no viewer associated", "An error has occured, and no viewer was associated with this widget. Please retry again later.");
		return;
	}

	QObject::connect(this->button_loadNewGridAPI, &QPushButton::clicked, this, &GridLoaderWidget::loadNewGridAPI);

	QObject::connect(this->button_loadGrids, &QPushButton::clicked, this, &GridLoaderWidget::loadGrid);
}

void GridLoaderWidget::loadNewGridAPI() {
	std::cerr << __PRETTY_FUNCTION__ << '\n';

	QStringList filenames = QFileDialog::getOpenFileNames(nullptr, "Open TIFF images (first channel)", this->basePath.path(), "TIFF files (*.tiff *.tif)", 0, QFileDialog::DontUseNativeDialog);
	if(filenames.empty()) {
	    QMessageBox* msgBox = new QMessageBox;
	    msgBox->setAttribute(Qt::WA_DeleteOnClose);
		msgBox->critical(this, "Error !", "No filenames provided !");
		return;
	}
	this->basePath.setPath(QFileInfo(filenames[0]).path());

    // TODO: bad tricks
    this->scene->filename = filenames[0].toStdString();

    int subsample = 1;
	if(this->group_subsample->isChecked()) {
        subsample = this->spinbox_subsample->value();
    }

	std::vector<std::string> filenamesAsString;
	for (int i = 0; i < filenames.size(); ++i) {
		filenamesAsString.push_back(filenames[i].toStdString());
	}

    //glm::vec3 sizeTetmesh = glm::vec3(20., 20., 20.);
    glm::vec3 sizeTetmesh = glm::vec3(this->spinbox_mesh_x->value(), this->spinbox_mesh_y->value(), this->spinbox_mesh_z->value());
    glm::vec3 sizeVoxel = glm::vec3(this->spinbox_voxel_x->value(), this->spinbox_voxel_y->value(), this->spinbox_voxel_z->value());

	if(this->group_bbox->isChecked()) {
        std::pair<glm::vec3, glm::vec3> bbox{glm::vec3(this->spinbox_bboxMin_x->value(), this->spinbox_bboxMin_y->value(), this->spinbox_bboxMin_z->value()), glm::vec3(this->spinbox_bboxMax_x->value(), this->spinbox_bboxMax_y->value(), this->spinbox_bboxMax_z->value())};
        std::cout << bbox.first << std::endl;
        std::cout << bbox.second << std::endl;
	    this->_testing_grid = new Grid(filenamesAsString, subsample, bbox);
    } else {
	    this->_testing_grid = new Grid(filenamesAsString, subsample);
    }
    this->_testing_grid->buildTetmesh(sizeTetmesh, sizeVoxel);
	this->viewer->newAPI_loadGrid(this->_testing_grid);
}

void GridLoaderWidget::loadGrid() {
	this->loadGrid_newAPI();
}

void GridLoaderWidget::loadGrid_newAPI() {
	// prevent changing the values of the inputs

	std::cerr << "Loading new grid API" << '\n';

	this->viewer->newAPI_loadGrid(this->_testing_grid);

	// Update min and max of the control panel
	// TODO: change this function in order to set slider according to min/max values in the image
	//this->_cp->setSlidersToNumericalLimits();

	this->close();
}
