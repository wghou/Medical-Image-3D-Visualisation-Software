#ifndef EXECUTABLES_ARAP_DEFORMATION_VIEWER_INCLUDE_ARAP_VIEWER_HPP_
#define EXECUTABLES_ARAP_DEFORMATION_VIEWER_INCLUDE_ARAP_VIEWER_HPP_

#include "../../features.hpp"
#include "../../qt/include/manipulator.hpp"

#include "../../meshes/operations/arap/Manipulator.h"
#include "../../meshes/operations/arap/RectangleSelection.h"
#include "../../meshes/operations/arap/PCATools.h"
#include "../../meshes/operations/arap/mesh_manip_interface.h"

#include "../../executables/arap_deformation/qt/include/arap_controller.hpp"

#include "../../../viewer/include/scene.hpp"

#include <QTimer>
#include <QPoint>

#include <memory>

/// @ingroup graphpipe
/// @brief A viewer that displays a scene, either in real space or in initial space
class Viewer : public QGLViewer {
	Q_OBJECT
public:
	/// @brief Default constructor for the viewer.
	Viewer(Scene* const scene, QStatusBar* program_bar, QWidget* parent = nullptr);
	~Viewer();
	/// @brief Multiplier to apply to scene radii for the scene's view.
	static float sceneRadiusMultiplier;

	/// @brief Updates info from the scene, binding its context for rendering.
	void updateInfoFromScene();

	/// @brief Add a pointer to the status bar, in order to show messages.
	void addStatusBar(QStatusBar* _sb);

public slots:
	/// @brief Slot called when the rectangular selection is used to add vertices.
	/// @param selection The rectangular area selected by the user, in screen coordinates.
	/// @param moving Should the vertices be added as movable or fixed handles ?
	void rectangleSelection_add(QRectF selection, bool moving);
	/// @brief Slot called when the rectangular selection is used to remove vertices.
	/// @param selection The selection made by the user.
	void rectangleSelection_remove(QRectF selection);
	/// @brief Slot called when the rectangle selection is applied
	void rectangleSelection_apply();
	/// @brief Slot called when the manipulator is moved.
	void arapManipulator_moved();
	/// @brief Slot called when the manipulator is released.
	void arapManipulator_released();

	/// @brief Sets the ARAP controller for the viewer.
	void setARAPController(ARAPController* arap_ctrl);

protected:
	/// @brief Initializes the scene, and the viewer's variables.
	virtual void init() override;
	/// @brief Draws the scene, in the space the viewer is supposed to show.
	virtual void draw() override;
	/// @brief Handles key events from the user.
	virtual void keyPressEvent(QKeyEvent* e) override;
	/// @brief Mouse press event generated by the widget.
	virtual void mousePressEvent(QMouseEvent* e) override;
	/// @brief Mouse press event generated by the widget.
	virtual void mouseMoveEvent(QMouseEvent* e) override;
	/// @brief Mouse press event generated by the widget.
	virtual void mouseReleaseEvent(QMouseEvent* e) override;
	/// @brief Wheel event for the mouse.
	virtual void wheelEvent(QWheelEvent* _w) override;
	/// @brief Defines the 'Help'/'About' string defined for this viewer.
	virtual QString helpString(void) const override;
	/// @brief Defines the 'Help'/'About' string for the keyboard for this viewer.
	virtual QString keyboardString(void) const override;
	/// @brief Defines the 'Help'/'About' string for the mouse for this viewer.
	virtual QString mouseString(void) const override;
	/// @brief Overrides the function to resize the widget.
	virtual void resizeGL(int w, int h) override;
	/// @brief Resets and removes the local point query
	void resetLocalPointQuery();

private:
	/// @brief The scene to control.
	Scene* const scene;
	/// @brief A refresh timer for the viewer, to update in real time.
	QTimer* refreshTimer;
	/// @brief The texture attached to the secondary framebuffer output.
	GLuint renderTarget;
	/// @brief Are we in "select mode" ? (for selecting coordinates in rasterized/volumetric view)
	bool selectMode;
	/// @brief Framebuffer dimensions, updated once per resize event
	glm::ivec2 fbSize;
	/// @brief Current cursor position relative to the widget's geometry origin
	glm::ivec2 cursorPos_current;
	/// @brief The number of frames a modifier (RMB,LMB,Ctrl ...) has been held for.
	std::size_t framesHeld;
	/// @brief Request to read a fragment"s position
	glm::ivec2 posRequest;
	/// @brief Program's status bar
	QStatusBar* statusBar;
	bool drawAxisOnTop;

	UITool::MeshManipulator meshManipulator;

	//
	// Stubs for ARAP integration
	//
	std::vector<glm::vec3> spheres;	   ///< Spheres to draw
	float sphere_size;	  ///< Sphere size in the renderer
	glm::vec3 temp_sphere_position;	   ///< Last-requested sphere position
	std::size_t temp_mesh_idx;	  ///< The mesh index selected
	std::size_t temp_mesh_vtx_idx;	  ///< The mesh vertex idx of the selected vertex (temp_sphere_position)
	std::size_t temp_img_idx;	 ///< The image index if found. WARNING : WE ASSUME IT IS ALWAYS 0, EVEN IF NO IMAGES ARE LOADED
	glm::vec3 temp_img_pos;	   ///< The position of that image index

	ARAPController* arap_controller;

	bool deformation_enabled;

public slots:
	/// @brief Update the view, as a slot without any arguments (currently only used by QTimer)
	void updateView() { this->update(); }
	/// @brief Updates the camera position once one or two grids are loaded in the scene.
	void updateCameraPosition(void);
	/// @brief Asks the scene to load a grid into itself.
	// void loadGrid(const std::shared_ptr<InputGrid>& g);
	/// @brief Asks the scene to load two grids into itself.
	// void loadTwoGrids(const std::shared_ptr<InputGrid>& g1, const std::shared_ptr<InputGrid>& g2);
	void newAPI_loadGrid(Image::Grid::Ptr ptr);
	/// @brief Re-centers the camera around the scene-defined center point
	void centerScene(void);
	/// @brief Guess the position of the fragment under the mouse cursor
	/// @param p The position read from the framebuffer.
	void guessMousePosition(glm::vec4 p);
	void toggleSelectionMode();
	void printVAOStateNext();
	void setDeformation(bool);

	/// @brief Receives a position from clicking on the framebuffer of one of the 2D viewers.
	void clickFromPlanarViewer(glm::vec4 coordinates);

	void updateSceneTextureLimits();

	void saveMesh();
	void saveCurve();

	void enableControlPanel(bool should_enable);

	/// @brief Read the pixel at the screen position given.
	glm::vec4 readPositionFromFramebuffer();

	void updateManipulatorsPositions(void);

	void toggleManipulators(void);

	void resetDeformation(void);

	/// @brief Called by the ARAPController in order to initialize the signals from its object to this viewer.
	void initializeARAPManipulationInterface();

signals:
	void enableDeformationPanel(bool should_enable);
	void hasInitializedScene();
	void enableImageControl(bool should_enable);
	void overrideTextureLimits(double min, double max);	// Override the current texture limits in the control panel
};

#endif	  // EXECUTABLES_ARAP_DEFORMATION_VIEWER_INCLUDE_ARAP_VIEWER_HPP_
