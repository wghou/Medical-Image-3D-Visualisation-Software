#include "../include/loader_widget.hpp"

#include "../../../image/transforms/include/affine_transform.hpp"
#include "../../../image/transforms/include/transform_interface.hpp"
#include "../../../image/transforms/include/transform_stack.hpp"
#include "../../../image/transforms/include/trs_transform.hpp"
#include "../../../qt/include/user_settings_widget.hpp"

#include <glm/gtx/io.hpp>

#include <QCoreApplication>
#include <QDialog>
#include <QFileDialog>
#include <QMessageBox>

#include <iomanip>

GridLoaderWidget::GridLoaderWidget(Scene* _scene, Viewer* _viewer, ControlPanel* cp, QWidget* parent) :
	QWidget(parent) {
	this->setAttribute(Qt::WA_DeleteOnClose);	 // delete widget and resources on close.
	this->basePath.setPath(QDir::homePath());
	this->scene					= _scene;
	this->viewer				= _viewer;
	this->_cp					= cp;
	this->dsLevel				= IO::DownsamplingLevel::Original;
	this->groupbox_userLimits	= nullptr;
	this->spinbox_userLimitMin	= nullptr;
	this->spinbox_userLimitMax	= nullptr;
	this->progress_load			= nullptr;
	this->_testing_grid			= nullptr;
	this->setupWidgets();
	this->setupLayouts();
	this->setupSignals();
}

GridLoaderWidget::~GridLoaderWidget() {
	this->scene	 = nullptr;
	this->viewer = nullptr;

	this->button_loadGrids->disconnect();

	delete this->groupbox_userLimits;

	delete this->label_headerLoader;
	delete this->label_headerTransformation;
	delete this->label_transformationAngle;
	delete this->label_transformationDimensions;

	delete this->button_loadGrids;

	delete this->dsb_transformationA;
	delete this->dsb_transformationDX;
	delete this->dsb_transformationDY;
	delete this->dsb_transformationDZ;

	delete this->layout_mainLayout;
	delete this->layout_transfoDetails;
}

void GridLoaderWidget::setupWidgets() {
	this->label_headerLoader			 = new QLabel("<h2><i>Load a grid</i></h2>");
	this->label_headerTransformation	 = new QLabel("Transformation details");
	this->label_transformationAngle		 = new QLabel("Angle of capture (degrees) : ");
	this->label_transformationDimensions = new QLabel("Physical resolution of a pixel (micrometers, on X, Y, and Z) :");
	this->label_gridInfoR				 = new QLabel("<No grid loaded>");

	this->progress_load			   = new QProgressBar;
	QSizePolicy retain_size_policy = this->progress_load->sizePolicy();
	retain_size_policy.setRetainSizeWhenHidden(true);
	this->progress_load->setSizePolicy(retain_size_policy);

	this->button_loadGrids		  = new QPushButton("Load grids into program");
	this->button_loadNewGridAPI2  = new QPushButton("Load with new grid API (one channel)");

	this->dsb_transformationA  = new QDoubleSpinBox;
	this->dsb_transformationDX = new QDoubleSpinBox;
	this->dsb_transformationDY = new QDoubleSpinBox;
	this->dsb_transformationDZ = new QDoubleSpinBox;

	this->dsb_offsetX = new QDoubleSpinBox;
	this->dsb_offsetY = new QDoubleSpinBox;
	this->dsb_offsetZ = new QDoubleSpinBox;

	// Angle from -180° to 180°, set to 0 by default :
	this->dsb_transformationA->setRange(-180., 180.);
	this->dsb_transformationA->setSingleStep(.5);
	this->dsb_transformationA->setValue(.0);
	// Voxel dimensions on X set to [0, 100] with 0 default (not loaded) :
	this->dsb_transformationDX->setRange(.0, 100.);
	this->dsb_transformationDX->setValue(1.);
	this->dsb_transformationDX->setSingleStep(.01);
	// Voxel dimensions on Y set to [0, 100] with 0 default (not loaded) :
	this->dsb_transformationDY->setRange(.0, 100.);
	this->dsb_transformationDY->setValue(1.);
	this->dsb_transformationDY->setSingleStep(.01);
	// Voxel dimensions on Z set to [0, 100] with 0 default (not loaded) :
	this->dsb_transformationDZ->setRange(.0, 100.);
	this->dsb_transformationDZ->setValue(1.);
	this->dsb_transformationDZ->setSingleStep(.01);

	// Offset can be negative, positive, and of any value. The default value is 0.0 (no need to set it).
	double min = std::numeric_limits<double>::lowest();
	double max = std::numeric_limits<double>::max();
	this->dsb_offsetX->setRange(min, max);
	this->dsb_offsetX->setSingleStep(.01);
	this->dsb_offsetY->setRange(min, max);
	this->dsb_offsetY->setSingleStep(.01);
	this->dsb_offsetZ->setRange(min, max);
	this->dsb_offsetZ->setSingleStep(.01);

	this->frame_transfoDetails = new QFrame;

	this->frame_transfoDetails->setFrameShape(QFrame::Shape::StyledPanel);
	this->frame_transfoDetails->setFrameShadow(QFrame::Shadow::Raised);

	this->groupBox_downsampling	  = new QGroupBox("Image resolution to load");
	this->groupBox_interpolator	  = new QGroupBox("Interpolation to use");
	this->groupbox_originalOffset = new QGroupBox("Sample position");
	this->groupbox_originalOffset->setToolTip("Original sample position relative to the microscope.");
	this->groupbox_originalOffset->setCheckable(true);
	this->groupbox_originalOffset->setChecked(false);

	this->radioButton_original = new QRadioButton("Original");
	this->radioButton_low	   = new QRadioButton("Low");
	this->radioButton_lower	   = new QRadioButton("Lower");
	this->radioButton_lowest   = new QRadioButton("Lowest");
	this->radioButton_nn	   = new QRadioButton("Nearest neighbor");
	this->radioButton_mean	   = new QRadioButton("Mean value");
	this->radioButton_mp	   = new QRadioButton("Most present value");
	this->radioButton_min	   = new QRadioButton("Min value");
	this->radioButton_max	   = new QRadioButton("Max value");

	this->groupbox_userLimits = new QGroupBox("Predefined minimum and maximum intensities to evaluate");
	this->groupbox_userLimits->setCheckable(true);
	this->groupbox_userLimits->setChecked(false);

	this->label_roiMin = new QLabel("Minimum intensity : ");
	this->label_roiMax = new QLabel("Maximum intensity : ");

	this->spinbox_userLimitMin = new QDoubleSpinBox;
	this->spinbox_userLimitMax = new QDoubleSpinBox;

	if (_testing_grid) {
		this->spinbox_userLimitMin->setMinimum(getMinNumericLimit(_testing_grid->getInternalDataType()));
		this->spinbox_userLimitMin->setMaximum(getMaxNumericLimit(_testing_grid->getInternalDataType()));
		this->spinbox_userLimitMin->setValue(getMinNumericLimit(_testing_grid->getInternalDataType()));
		this->spinbox_userLimitMax->setMinimum(getMinNumericLimit(_testing_grid->getInternalDataType()));
		this->spinbox_userLimitMax->setMaximum(getMaxNumericLimit(_testing_grid->getInternalDataType()));
		this->spinbox_userLimitMax->setValue(getMaxNumericLimit(_testing_grid->getInternalDataType()));
	} else {
		this->spinbox_userLimitMin->setMinimum(0);
		this->spinbox_userLimitMin->setMaximum(10);
		this->spinbox_userLimitMin->setValue(0);
		this->spinbox_userLimitMax->setMinimum(0);
		this->spinbox_userLimitMax->setMaximum(10);
		this->spinbox_userLimitMax->setValue(10);
	}

	this->groupBox_interpolator->setDisabled(true);
	this->radioButton_original->setChecked(true);
	this->radioButton_nn->setChecked(true);
}

void GridLoaderWidget::setupLayouts() {
	this->layout_mainLayout		= new QVBoxLayout;
	this->layout_transfoDetails = new QGridLayout;
	this->layout_downsampling	= new QHBoxLayout;
	this->layout_interpolator	= new QGridLayout;
	this->layout_roiSelection	= new QGridLayout;

	this->layout_gb_offset = new QHBoxLayout;

	// layout and frame for the transformation details :
	this->layout_transfoDetails->addWidget(this->label_transformationAngle, 0, 0, 1, 2);
	this->layout_transfoDetails->addWidget(this->dsb_transformationA, 0, 2, 1, 1);
	this->layout_transfoDetails->addWidget(this->label_transformationDimensions, 1, 0, 1, 3);
	this->layout_transfoDetails->addWidget(this->dsb_transformationDX, 2, 0, 1, 1);
	this->layout_transfoDetails->addWidget(this->dsb_transformationDY, 2, 1, 1, 1);
	this->layout_transfoDetails->addWidget(this->dsb_transformationDZ, 2, 2, 1, 1);
	this->frame_transfoDetails->setLayout(this->layout_transfoDetails);

	//downsampling layout :
	this->layout_downsampling->addWidget(this->radioButton_original);
	this->layout_downsampling->addWidget(this->radioButton_low);
	this->layout_downsampling->addWidget(this->radioButton_lower);
	this->layout_downsampling->addWidget(this->radioButton_lowest);
	this->groupBox_downsampling->setLayout(this->layout_downsampling);
	// interpolator layout :
	this->layout_interpolator->addWidget(this->radioButton_nn, 0, 0);
	this->layout_interpolator->addWidget(this->radioButton_mean, 0, 1);
	this->layout_interpolator->addWidget(this->radioButton_mp, 0, 2);
	this->layout_interpolator->addWidget(this->radioButton_min, 1, 0);
	this->layout_interpolator->addWidget(this->radioButton_max, 1, 1);
	this->groupBox_interpolator->setLayout(this->layout_interpolator);

	// ROI selection layout :
	this->layout_roiSelection->addWidget(this->label_roiMin, 0, 0);
	this->layout_roiSelection->addWidget(this->spinbox_userLimitMin, 0, 1);
	this->layout_roiSelection->addWidget(this->label_roiMax, 1, 0);
	this->layout_roiSelection->addWidget(this->spinbox_userLimitMax, 1, 1);
	this->groupbox_userLimits->setLayout(this->layout_roiSelection);

	// Offset input layout :
	this->layout_gb_offset->addWidget(this->dsb_offsetX);
	this->layout_gb_offset->addWidget(this->dsb_offsetY);
	this->layout_gb_offset->addWidget(this->dsb_offsetZ);
	this->groupbox_originalOffset->setLayout(this->layout_gb_offset);

	// main layout :
	this->layout_mainLayout->addWidget(this->label_headerLoader, 0, Qt::AlignCenter);
	this->layout_mainLayout->addWidget(this->button_loadNewGridAPI2);
	this->layout_mainLayout->addWidget(this->groupBox_downsampling);
	this->layout_mainLayout->addWidget(this->groupBox_interpolator);
	this->layout_mainLayout->addWidget(this->groupbox_userLimits);
	this->layout_mainLayout->addWidget(this->groupbox_originalOffset);
	this->layout_mainLayout->addStretch(1);
	this->layout_mainLayout->addWidget(this->label_gridInfoR);
	this->layout_mainLayout->addStretch(1);
	this->layout_mainLayout->addWidget(this->frame_transfoDetails);
	this->layout_mainLayout->addStretch(1);
	this->layout_mainLayout->addWidget(this->progress_load);
	this->layout_mainLayout->addWidget(this->button_loadGrids);

	// set main layout :
	this->setLayout(this->layout_mainLayout);

	this->progress_load->hide();
}

void GridLoaderWidget::setupSignals() {
	// check if scene and viewer are valid before setting up the signals :
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

	// load from DIM/IMA/TIFF files :
	QObject::connect(this->button_loadNewGridAPI2, &QPushButton::clicked, this, &GridLoaderWidget::loadNewGridAPI1Channel);

	// load grid into mem :
	QObject::connect(this->button_loadGrids, &QPushButton::clicked, this, &GridLoaderWidget::loadGrid);

	QObject::connect(this->radioButton_original, &QRadioButton::toggled, [this]() {
		this->resetGridInfoLabel();
		if (this->radioButton_original->isChecked()) {
			this->dsLevel = IO::DownsamplingLevel::Original;
			this->groupBox_interpolator->setEnabled(false);
		}
		this->computeGridInfoLabel();
	});
	QObject::connect(this->radioButton_low, &QRadioButton::toggled, [this]() {
		this->resetGridInfoLabel();
		if (this->radioButton_low->isChecked()) {
			this->dsLevel = IO::DownsamplingLevel::Low;
			this->groupBox_interpolator->setEnabled(true);
		}
		this->computeGridInfoLabel();
	});
	QObject::connect(this->radioButton_lower, &QRadioButton::toggled, [this]() {
		this->resetGridInfoLabel();
		if (this->radioButton_lower->isChecked()) {
			this->dsLevel = IO::DownsamplingLevel::Lower;
			this->groupBox_interpolator->setEnabled(true);
		}
		this->computeGridInfoLabel();
	});
	QObject::connect(this->radioButton_lowest, &QRadioButton::toggled, [this]() {
		this->resetGridInfoLabel();
		if (this->radioButton_lowest->isChecked()) {
			this->dsLevel = IO::DownsamplingLevel::Lowest;
			this->groupBox_interpolator->setEnabled(true);
		}
		this->computeGridInfoLabel();
	});
}

void GridLoaderWidget::disableWidgets() {
	this->button_loadGrids->setDisabled(true);

	this->dsb_transformationA->setDisabled(true);
	this->dsb_transformationDX->setDisabled(true);
	this->dsb_transformationDY->setDisabled(true);
	this->dsb_transformationDZ->setDisabled(true);

	this->frame_transfoDetails->setDisabled(true);

	this->groupBox_downsampling->setDisabled(true);
	this->groupBox_interpolator->setDisabled(true);
	this->groupbox_userLimits->setDisabled(true);
	this->groupbox_originalOffset->setDisabled(true);

	this->label_headerLoader->setDisabled(true);
	this->label_headerTransformation->setDisabled(true);
	this->label_transformationAngle->setDisabled(true);
	this->label_transformationDimensions->setDisabled(true);
	this->label_gridInfoR->setDisabled(true);

	this->radioButton_original->setDisabled(true);
	this->radioButton_low->setDisabled(true);
	this->radioButton_lower->setDisabled(true);
	this->radioButton_lowest->setDisabled(true);
	this->radioButton_nn->setDisabled(true);
	this->radioButton_mean->setDisabled(true);
	this->radioButton_mp->setDisabled(true);
	this->radioButton_min->setDisabled(true);
	this->radioButton_max->setDisabled(true);
}

void GridLoaderWidget::setWidgetsEnabled(bool _enabled) {
	this->button_loadGrids->setEnabled(_enabled);

	this->dsb_transformationA->setEnabled(_enabled);
	this->dsb_transformationDX->setEnabled(_enabled);
	this->dsb_transformationDY->setEnabled(_enabled);
	this->dsb_transformationDZ->setEnabled(_enabled);

	this->frame_transfoDetails->setEnabled(_enabled);

	this->groupBox_downsampling->setEnabled(_enabled);
	this->groupBox_interpolator->setEnabled(_enabled);
	this->groupbox_userLimits->setEnabled(_enabled);
	this->groupbox_originalOffset->setEnabled(_enabled);

	this->label_headerLoader->setEnabled(_enabled);
	this->label_headerTransformation->setEnabled(_enabled);
	this->label_transformationAngle->setEnabled(_enabled);
	this->label_transformationDimensions->setEnabled(_enabled);
	this->label_gridInfoR->setEnabled(_enabled);

	this->radioButton_original->setEnabled(_enabled);
	this->radioButton_low->setEnabled(_enabled);
	this->radioButton_lower->setEnabled(_enabled);
	this->radioButton_lowest->setEnabled(_enabled);
	this->radioButton_nn->setEnabled(_enabled);
	this->radioButton_mean->setEnabled(_enabled);
	this->radioButton_mp->setEnabled(_enabled);
	this->radioButton_min->setEnabled(_enabled);
	this->radioButton_max->setEnabled(_enabled);
}

void GridLoaderWidget::resetGridInfoLabel() {
	this->label_gridInfoR->setText("Loading grid information ...");
	return;
}

void GridLoaderWidget::computeGridInfoLabel() {
	if (this->_testing_grid != nullptr) {
		auto dims		 = this->_testing_grid->getResolution();
		auto v			 = this->_testing_grid->getVoxelDimensionality();
		QString infogrid = "Image dimensions : " + QString::number(dims.x) + "x" + QString::number(dims.y) + "x" +
						   QString::number(dims.z) + " on " + QString::number(v) + " channels.";
		this->label_gridInfoR->setText(infogrid);
		return;
	}
}

void GridLoaderWidget::updateVoxelDimensions_silent() {
	// NOTE : we only rely on the red reader's voxel dimensions, because we assume
	// the voxel dimensions _do not change_ between color channels.

	auto vxDims = this->_testing_grid->getVoxelDimensions();
	this->dsb_transformationDX->blockSignals(true);
	this->dsb_transformationDY->blockSignals(true);
	this->dsb_transformationDZ->blockSignals(true);

	this->dsb_transformationDX->setValue(vxDims.x);
	this->dsb_transformationDY->setValue(vxDims.y);
	this->dsb_transformationDZ->setValue(vxDims.z);

	this->dsb_transformationDZ->blockSignals(false);
	this->dsb_transformationDY->blockSignals(false);
	this->dsb_transformationDX->blockSignals(false);

	return;
}

void GridLoaderWidget::progressBar_init_undefined(QString format_message) {
	assert(this->progress_load != nullptr);
	// set a new progress bar :
	this->progress_load->setRange(0, 0);
	this->progress_load->setValue(0);
	this->progress_load->setFormat(format_message);
	if (not this->progress_load->isVisible()) {
		this->progress_load->setVisible(true);
	}
	// update and show the bar :
	//QCoreApplication::processEvents();
	//this->update();
}

void GridLoaderWidget::progressBar_init_defined(int min, int max, int current_value, QString format_string) {
	this->progress_load->setRange(min, max);
	this->progress_load->setValue(current_value);
	this->progress_load->setFormat(format_string);
	if (not this->progress_load->isVisible()) {
		this->progress_load->setVisible(true);
	}
	// update and show the bar :
	QCoreApplication::processEvents();
	this->update();
}

void GridLoaderWidget::progressBar_reset() {
	this->progress_load->reset();
	if (this->progress_load->isVisible()) {
		this->progress_load->setVisible(false);
	}
	QCoreApplication::processEvents();
	this->update();
}

void GridLoaderWidget::loadNewGridAPI1Channel() {
	std::cerr << __PRETTY_FUNCTION__ << '\n';

	// if an error occurs :
	QMessageBox* msgBox = new QMessageBox;
	msgBox->setAttribute(Qt::WA_DeleteOnClose);

	QStringList filenamesR = QFileDialog::getOpenFileNames(nullptr, "Open TIFF images (first channel)", this->basePath.path(), "TIFF files (*.tiff *.tif)", 0, QFileDialog::DontUseNativeDialog);
	if (filenamesR.empty()) {
		msgBox->critical(this, "Error !", "No filenames provided !");
		this->computeGridInfoLabel();
		return;
	}
	// update path from last file picker :
	this->basePath.setPath(QFileInfo(filenamesR[0]).path());

	// disable widgets, in order to let the parsing function actually start !!!
	this->setWidgetsEnabled(false);

	// set a undefined progress bar to the screen, showing initialization of the
	this->progressBar_init_undefined("Initializing image reader ...");

	std::vector<std::string> fnR;	 // filenames, red channel
	for (int i = 0; i < filenamesR.size(); ++i) {
		fnR.push_back(filenamesR[i].toStdString());
	}

	// Try to create a grid :
	std::cerr << "Trying to create a grid !!!\n";
	std::vector<std::vector<std::string>> fnames_grid{fnR};
	// FIXME : The API has changed. Grid::createGrid should only return an empty TIFF implementation, and
	// the parsing function is the one to take the files as an input.
	this->_testing_grid = Image::Grid::createGrid(fnames_grid);
	if (this->_testing_grid == nullptr) {
		std::cerr << "Error : createGrid() returned nullptr !\n";
		return;
	}

	// Start parsing the information :
	std::cerr << "Grid created, updating info from disk ...\n";
	auto task = this->_testing_grid->updateInfoFromDisk(fnames_grid);

	// may be already ended with errors, show them :
	std::string before_error_message;
	std::string full_msg;
	if (task->popMessage(before_error_message)) {
		do {
			full_msg += before_error_message + '\n';
		} while (task->popMessage(before_error_message));
		msgBox->critical(this, "Error while parsing files !", QString(before_error_message.c_str()));
		this->setWidgetsEnabled();
		return;
	}

	// Set and show progress bar :
	this->progressBar_init_defined(0, task->getMaxSteps(), 0, "Parsing image data ... (%p%)");

	// Parse the grid :
	do {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		std::size_t steps = task->getMaxSteps();
		std::size_t adv	  = task->getAdvancement();
		this->progress_load->setRange(0, steps);
		this->progress_load->setValue(adv);
		this->progress_load->setFormat("Parsing image data ... (%p%)");
		// Needed to update the main window ...
		QCoreApplication::processEvents();
		this->update();
	} while (not task->isComplete());

	std::cerr << "\n[TASK] Done parsing the grid ... \n";
	std::string errmsg = "";
	std::string all_errors;
	bool isComplete = true;
	while (task->popMessage(errmsg)) {
		std::cerr << "[Task error] Message : " << errmsg << '\n';
		isComplete = false;
		all_errors += errmsg + '\n';
	}
	if (not all_errors.empty()) {
		msgBox->critical(this, "Errors while parsing the files", QString(all_errors.c_str()));
		this->setWidgetsEnabled();
		return;
	}
	if (isComplete) {
		this->_testing_grid->updateInfoFromGrid();
		std::cerr << "Grid dimensionality : " << this->_testing_grid->getVoxelDimensionality() << '\n';
		Image::svec3 res = this->_testing_grid->getResolution();
		glm::vec3 vx	 = this->_testing_grid->getVoxelDimensions();
		std::cerr << "Grid resolution : " << res.x << ", " << res.y << ", " << res.z << "\n";
		std::cerr << "Voxel dimensions : " << vx.x << ", " << vx.y << ", " << vx.z << "\n";
		std::cerr << "Data internal representation : " << this->_testing_grid->getInternalDataType() << '\n';
	}

	this->computeGridInfoLabel();

	this->progressBar_reset();
	this->setWidgetsEnabled();
}

void GridLoaderWidget::loadGrid() {
	this->loadGrid_newAPI();
}

void GridLoaderWidget::loadGrid_newAPI() {
	// prevent changing the values of the inputs
	this->disableWidgets();

	bool hasUserBounds = this->groupbox_userLimits->isChecked();
	// Impossible with the new API as the data_t data type isn't known at compile time
	// DiscreteGrid::data_t userMin = static_cast<DiscreteGrid::data_t>(this->spinbox_userLimitMin->value());
	// DiscreteGrid::data_t userMax = static_cast<DiscreteGrid::data_t>(this->spinbox_userLimitMax->value());
	std::cerr << "Loading new grid API" << '\n';

	float dx = this->dsb_transformationDX->value();
	float dy = this->dsb_transformationDY->value();
	float dz = this->dsb_transformationDZ->value();

#warning TODO : Enable downsampling options from grid here
#warning TODO : Enable user voxel sizes here (via a transformation matrix ?)
#warning TODO : Enable computation of approximate voxel size here
#warning TODO : Fetch downsampling type from the type of radio button checked

	glm::vec3 vxdims = glm::vec3(
	  static_cast<float>(this->dsb_transformationDX->value()),
	  static_cast<float>(this->dsb_transformationDY->value()),
	  static_cast<float>(this->dsb_transformationDZ->value()));
	//svec3 dims = this->inputGridR->getResolution();
	float a				= this->dsb_transformationA->value();
	auto transfo_matrix = computeTransfoShear_newAPI(a, this->_testing_grid, vxdims);

	MatrixTransform::Ptr grid_transform = std::make_shared<MatrixTransform>(transfo_matrix);
	this->_testing_grid->addTransform(grid_transform);

	Image::GridRepo repo = Image::GridRepo::getInstance();
	repo.addGrid(this->_testing_grid);

	// Here, load the downsampled grid if necessary :
	if (this->dsLevel != IO::DownsamplingLevel::Original) {
		svec3 target_resolution = this->_testing_grid->getResolution();
		if (this->dsLevel == IO::DownsamplingLevel::Low) {
			target_resolution /= 2u;
		} else if (this->dsLevel == IO::DownsamplingLevel::Lower) {
			target_resolution /= 4u;
		} else if (this->dsLevel == IO::DownsamplingLevel::Lowest) {
			target_resolution /= 8u;
		}
		std::cerr << "Downsampling applied, target resolution is : [" << target_resolution << "]\n";

		Image::Grid::Ptr downsampled_grid = this->_testing_grid->requestDownsampledVersion(target_resolution, Image::ImageResamplingTechnique::NearestNeighbor);

		// Attempt to load the grid :
		// TODO : very hacky code, not for release purposes. Should be sanitized and commented more.
		std::cerr << "Attempting to parse image data ...\n";
		QMessageBox* msgBox = new QMessageBox;
		msgBox->setAttribute(Qt::WA_DeleteOnClose);
		// Try to create a grid :
		std::cerr << "Trying to create a grid !!!\n";
		if (downsampled_grid == nullptr) {
			std::cerr << "Error : Grid::requestDownsampledVersion() returned nullptr !\n";
			return;
		}

		// Start parsing the information :
		std::cerr << "Grid created, updating info from disk ...\n";
		auto task = downsampled_grid->updateInfoFromDisk(std::vector<std::vector<std::string>>{});

		// may be already ended with errors, show them :
		std::string before_error_message;
		std::string full_msg;
		if (task->popMessage(before_error_message)) {
			do {
				full_msg += before_error_message + '\n';
			} while (task->popMessage(before_error_message));
			msgBox->critical(this, "Error while parsing files !", QString(before_error_message.c_str()));
			this->setWidgetsEnabled();
			return;
		}

		// Set and show progress bar :
		this->progressBar_init_defined(0, task->getMaxSteps(), 0, "Parsing image data ... (%p%)");

		// Parse the grid :
		do {
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			std::size_t steps = task->getMaxSteps();
			std::size_t adv	  = task->getAdvancement();
			this->progress_load->setRange(0, steps);
			this->progress_load->setValue(adv);
			this->progress_load->setFormat("Parsing image data ... (%p%)");
			// Needed to update the main window ...
			QCoreApplication::processEvents();
			this->update();
		} while (not task->isComplete());

		std::cerr << "\n[TASK] Done parsing the grid ... \n";
		std::string errmsg = "";
		std::string all_errors;
		bool isComplete = true;
		while (task->popMessage(errmsg)) {
			std::cerr << "[Task error] Message : " << errmsg << '\n';
			isComplete = false;
			all_errors += errmsg + '\n';
		}
		if (not all_errors.empty()) {
			msgBox->critical(this, "Errors while parsing the files", QString(all_errors.c_str()));
			this->setWidgetsEnabled();
			return;
		}
		if (isComplete) {
			downsampled_grid->updateInfoFromGrid();
			std::cerr << "Grid dimensionality : " << downsampled_grid->getVoxelDimensionality() << '\n';
			Image::svec3 res = downsampled_grid->getResolution();
			glm::vec3 vx	 = downsampled_grid->getVoxelDimensions();
			std::cerr << "Grid resolution : " << res.x << ", " << res.y << ", " << res.z << "\n";
			std::cerr << "Voxel dimensions : " << vx.x << ", " << vx.y << ", " << vx.z << "\n";
			std::cerr << "Data internal representation : " << downsampled_grid->getInternalDataType() << '\n';
		}

		repo.addGrid(downsampled_grid);
		//this->_cp->setSlidersToNumericalLimits();
	} else {
		// Load the grid data, and make a copy here

		// Update min and max of the control panel
		// TODO: change this function in order to set slider according to min/max values in the image
		//this->_cp->setSlidersToNumericalLimits();
	}

	emit newAPIGridLoaded(this->_testing_grid);

	this->close();
}

glm::mat4 computeTransfoShear_newAPI(double angleDeg, const Image::Grid::Ptr& grid, glm::vec3 vxdims) {
	glm::mat4 transfoMat = glm::mat4(1.0);

	double angleRad = (angleDeg * M_PI) / 180.;

	transfoMat[0][0] = /* vxdims.x */ std::cos(angleRad);
	transfoMat[0][2] = /* vxdims.x */ std::sin(angleRad);
	transfoMat[1][1] = /* vxdims.y */ 1.f;
	transfoMat[2][2] = /* vxdims.z */ std::cos(angleRad);

	/*
	if (angleDeg < 0.) {
		auto dims = grid->getBoundingBox().getDiagonal();
		// compute translation along Z :
		float w = static_cast<float>(dims.x); //vxdims.x;
		float displacement = w * std::abs(std::sin(angleRad));
		transfoMat = glm::translate(transfoMat, glm::vec3(.0, .0, displacement));
	}
	*/

	return transfoMat;
}