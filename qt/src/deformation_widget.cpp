#include "../include/deformation_widget.hpp"

#include "../../../image/transforms/include/affine_transform.hpp"
#include "../../../image/transforms/include/transform_interface.hpp"
#include "../../../image/transforms/include/transform_stack.hpp"
#include "../../../image/transforms/include/trs_transform.hpp"
#include "../include/user_settings_widget.hpp"

#include <glm/gtx/io.hpp>

#include <QCoreApplication>
#include <QDialog>
#include <QFileDialog>
#include <QMessageBox>

#include <iomanip>

GridDeformationWidget::GridDeformationWidget(Scene* scene, QWidget* parent) :
	QWidget(parent) {

    this->setWindowFlags(Qt::WindowStaysOnTopHint);

	this->group_mesh = new QGroupBox("Mesh to select");
	this->radio_mesh_grid_1 = new QRadioButton("Grid 1");
	this->radio_mesh_grid_1->setChecked(true);
	this->radio_mesh_grid_2 = new QRadioButton("Grid 2");
	this->radio_mesh_grid_2->setChecked(false);
	this->radio_mesh_surface = new QRadioButton("Surface");
	this->radio_mesh_surface->setChecked(false);

	this->group_selector = new QGroupBox("Selector mode");
	this->radio_selector_direct = new QRadioButton("Direct");
	this->radio_selector_direct->setChecked(true);
	this->radio_selector_free = new QRadioButton("Free");
	this->radio_selector_free->setChecked(false);
	this->radio_selector_position = new QRadioButton("Position");
	this->radio_selector_position->setChecked(false);

	this->group_move = new QGroupBox("Move method");
	this->radio_move_normal = new QRadioButton("Normal");
	this->radio_move_normal->setChecked(true);
	this->radio_move_weighted = new QRadioButton("Weighted");
	this->radio_move_weighted->setChecked(false);

	this->label_radius_selection = new QLabel("Selection radius");
	this->spinbox_radius_selection = new QDoubleSpinBox;
    this->spinbox_radius_selection->setValue(30.);
	this->spinbox_radius_selection->setRange(0., 20000.);
	this->spinbox_radius_selection->setSingleStep(10.);

	this->label_radius_sphere = new QLabel("Sphere radius");
	this->spinbox_radius_sphere = new QDoubleSpinBox;
    this->spinbox_radius_sphere->setValue(5.);
	this->spinbox_radius_sphere->setRange(0., 20000.);
	this->spinbox_radius_sphere->setSingleStep(1.);

	this->label_wireframe = new QLabel("Display wireframe");
	this->checkbox_wireframe = new QCheckBox;
    this->checkbox_wireframe->setChecked(true);

    this->debug_button = new QPushButton("&DEBUG", this);
    this->debug_it = new QPushButton("&ITERATION", this);
    this->debug_init = new QPushButton("&INIT", this);

	this->spinbox_l_selection = new QDoubleSpinBox;
    this->spinbox_l_selection->setValue(0.);
	this->spinbox_l_selection->setRange(0., 20000.);
	this->spinbox_l_selection->setSingleStep(1.);

	this->spinbox_N_selection = new QDoubleSpinBox;
    this->spinbox_N_selection->setValue(0.);
	this->spinbox_N_selection->setRange(0., 20000.);
	this->spinbox_N_selection->setSingleStep(1.);

	this->spinbox_S_selection = new QDoubleSpinBox;
    this->spinbox_S_selection->setValue(0.);
	this->spinbox_S_selection->setRange(0., 20000.);
	this->spinbox_S_selection->setSingleStep(1.);

    this->setupLayouts();
	this->setupSignals(scene);
}

void GridDeformationWidget::setupLayouts() {

    this->mainLayout = new QVBoxLayout;

	layout_mesh     = new QHBoxLayout;
	layout_selector = new QHBoxLayout;
	layout_move     = new QHBoxLayout;

	this->group_mesh->setLayout(this->layout_mesh);
	this->group_selector->setLayout(this->layout_selector);
	this->group_move->setLayout(this->layout_move);

	this->layout_mesh->addWidget(this->radio_mesh_grid_1, 1);
	this->layout_mesh->addWidget(this->radio_mesh_grid_2, 2);
	this->layout_mesh->addWidget(this->radio_mesh_surface, 3);

	this->layout_selector->addWidget(this->radio_selector_direct, 1);
	this->layout_selector->addWidget(this->radio_selector_free, 2);
	this->layout_selector->addWidget(this->radio_selector_position, 3);

	this->layout_move->addWidget(this->radio_move_normal, 1);
	this->layout_move->addWidget(this->radio_move_weighted, 2);

    this->mainLayout->addWidget(this->group_mesh);
    this->mainLayout->addWidget(this->group_selector);
    this->mainLayout->addWidget(this->group_move);

    this->mainLayout->addWidget(this->label_radius_selection);
    this->mainLayout->addWidget(this->spinbox_radius_selection);

    this->mainLayout->addWidget(this->label_radius_sphere);
    this->mainLayout->addWidget(this->spinbox_radius_sphere);

    this->mainLayout->addWidget(this->label_wireframe);
    this->mainLayout->addWidget(this->checkbox_wireframe);

    this->mainLayout->addWidget(this->debug_button);
    this->mainLayout->addWidget(this->debug_init);
    this->mainLayout->addWidget(this->debug_it);
    this->mainLayout->addWidget(this->spinbox_l_selection);
    this->mainLayout->addWidget(this->spinbox_N_selection);
    this->mainLayout->addWidget(this->spinbox_S_selection);

    this->setLayout(this->mainLayout);
}

GridDeformationWidget::~GridDeformationWidget() {
}

void GridDeformationWidget::setupSignals(Scene* scene) {
	QObject::connect(this->radio_mesh_grid_1, &QPushButton::clicked, this, [this, scene]() {this->useSurface = false; scene->sendTetmeshToGPU(0, InfoToSend(InfoToSend::VERTICES | InfoToSend::NORMALS | InfoToSend::TEXCOORD | InfoToSend::NEIGHBORS)); scene->gridToDraw = 0;});

	QObject::connect(this->radio_mesh_grid_2, &QPushButton::clicked, this, [this, scene]() {this->useSurface = false; scene->sendTetmeshToGPU(1, InfoToSend(InfoToSend::VERTICES | InfoToSend::NORMALS | InfoToSend::TEXCOORD | InfoToSend::NEIGHBORS)); scene->gridToDraw = 1;});

	QObject::connect(this->radio_mesh_surface, &QPushButton::clicked, this, [this, scene]() {this->useSurface = true;});

	QObject::connect(this->radio_selector_direct, &QPushButton::clicked, this, [this, scene]() {scene->createNewMeshManipulator(0, this->useSurface);});

	QObject::connect(this->radio_selector_free, &QPushButton::clicked, this, [this, scene]() {scene->createNewMeshManipulator(1, this->useSurface);});

	QObject::connect(this->radio_selector_position, &QPushButton::clicked, this, [this, scene]() {scene->createNewMeshManipulator(2, this->useSurface);});

	QObject::connect(this->spinbox_radius_sphere, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [=](double i){ scene->setManipulatorRadius(i);}); 

	QObject::connect(this->spinbox_l_selection, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [=](double i){ scene->setL(i);}); 
	QObject::connect(this->spinbox_N_selection, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [=](double i){ scene->setN(i);}); 
	QObject::connect(this->spinbox_S_selection, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [=](double i){ scene->setS(i);}); 

	QObject::connect(this->checkbox_wireframe, &QPushButton::clicked, this, [this, scene]() {scene->toggleWireframe();});

	QObject::connect(this->radio_move_normal, &QPushButton::clicked, this, [this, scene]() {scene->setNormalDeformationMethod();});

	QObject::connect(this->radio_move_weighted, &QPushButton::clicked, this, [this, scene]() {scene->setWeightedDeformationMethod(this->spinbox_radius_selection->value());});

    QObject::connect(this->debug_button, &QPushButton::released, this, [this, scene]() {scene->createNewICP();});

    QObject::connect(this->debug_it, &QPushButton::released, this, [this, scene]() {scene->ICPIteration();});

    QObject::connect(this->debug_init, &QPushButton::released, this, [this, scene]() {scene->ICPInitialize();});
}
