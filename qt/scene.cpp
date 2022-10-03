#include "scene.hpp"
//#include "planar_viewer.hpp"

#include <GL/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/io.hpp>
#include <glm/gtx/transform.hpp>

#include <QFileDialog>
#include <QMessageBox>
#include <QOpenGLContext>
#include <QSurface>
#include <limits>

#include <fstream>
#include <type_traits>

#include <chrono>
#include <utility>
#include <map>
#include <vector>

#include "../../grid/geometry/grid.hpp"
#include "../../grid/deformation/mesh_deformer.hpp"

#include "../../grid/utils/apss.hpp"
#include "grid/geometry/base_mesh.hpp"
#include "grid/geometry/surface_mesh.hpp"
#include "grid/ui/mesh_manipulator.hpp"

inline void __GetTexSize(std::size_t numTexNeeded, std::size_t* opt_width, std::size_t* opt_height) {
    std::size_t iRoot = (std::size_t) sqrtf(static_cast<float>(numTexNeeded));

    *opt_width	= iRoot + 1;
    *opt_height = iRoot;
    if (((*opt_width) * (*opt_height)) < numTexNeeded) {
        (*opt_height)++;
    }
    if (((*opt_width) * (*opt_height)) < numTexNeeded) {
        (*opt_width)++;
    }
}

/** This constructor not only creates the object, but also sets the default values for the Scene in order
 *  to be drawn, even if it is empty at the time of the first call to a draw function.
 */
Scene::Scene() {
    this->displayTetmesh = false;
    this->meshManipulator = nullptr;
    this->distanceFromCamera = 0.;
    this->cameraPosition = glm::vec3(0., 0., 0.);

    //this->grids.clear();

    this->context			= nullptr;
    this->controlPanel		= nullptr;
    //this->gridControl		= nullptr;
    this->programStatusBar	= nullptr;

    double minTexVal		= 1;
    double maxTexVal		= std::numeric_limits<int>::max();
    this->textureBounds0	= GridGLView::data_2(minTexVal, maxTexVal);
    this->textureBounds1	= GridGLView::data_2(minTexVal, maxTexVal);
    this->colorBounds0		= GridGLView::data_2(0, maxTexVal);
    this->colorBounds1		= GridGLView::data_2(0, maxTexVal);
    this->selectedChannel_r = 0;	// by default, the R channel
    this->selectedChannel_g = 0;	// by default, the R channel

    // Default light positions : at the vertices of a unit cube.
    this->lightPositions = {
      glm::vec3(.0, .0, .0), glm::vec3(1., .0, .0),
      glm::vec3(.0, 1., .0), glm::vec3(1., 1., .0),
      glm::vec3(.0, .0, 1.), glm::vec3(1., .0, 1.),
      glm::vec3(.0, 1., 1.), glm::vec3(1., 1., 1.)};

    this->planeDirection	= glm::vec3(1., 1., 1.);
    this->planeDisplacement = glm::vec3(.0, .0, .0);
    Image::bbox_t::vec min(.0, .0, .0);
    Image::bbox_t::vec max(1., 1., 1.);
    this->sceneBB				 = Image::bbox_t(min, max);
    this->clipDistanceFromCamera = 5.f;
    this->rgbMode				 = ColorChannel::HandEColouring;
    this->channels_r			 = ColorFunction::SingleChannel;
    this->channels_g			 = ColorFunction::SingleChannel;

    // Show all planes as default :
    this->planeVisibility = glm::vec<3, bool, glm::defaultp>(true, true, true);

    this->vao					  = 0;
    this->vao_VolumetricBuffers = 0;
    this->vao_boundingBox		  = 0;

    this->vbo_VertPos				= 0;
    this->vbo_VertNorm			= 0;
    this->vbo_VertTex				= 0;
    this->vbo_Element				= 0;
    this->vbo_boundingBoxVertices = 0;
    this->vbo_boundingBoxIndices	= 0;

    this->program_projectedTex	 = 0;
    this->program_Plane3D			 = 0;
    this->program_PlaneViewer		 = 0;
    this->program_VolumetricViewer = 0;
    this->program_BoundingBox		 = 0;

    this->tex_ColorScaleGrid			= 0;
    this->tex_ColorScaleGridAlternate = 0;
    this->vbo_Texture3D_VertPos		= 0;
    this->vbo_Texture3D_VertNorm		= 0;
    this->vbo_Texture3D_VertTex		= 0;
    this->vbo_Texture3D_VertIdx		= 0;

    this->state_idx = 0;
    this->pos_idx = 0;

    this->color0		= glm::vec3(1., .0, .0);
    this->color1		= glm::vec3(.0, .0, 1.);
    this->color0_second = glm::vec3(1., .0, .0);
    this->color1_second = glm::vec3(.0, .0, 1.);

    std::cerr << "Allocating " << +std::numeric_limits<GridGLView::data_t>::max() << " elements for vis ...\n";

    this->shouldUpdateUserColorScales = false;
    this->shouldUpdateUBOData		  = false;

    this->glSelection = new UITool::GL::Selection(&this->sceneGL, glm::vec3(0., 0., 0.), glm::vec3(10., 10., 10.));

    this->currentTool = UITool::MeshManipulatorType::POSITION;
    this->currentDeformMethod = DeformMethod::NORMAL;
    this->planeActivation = glm::vec3(1., 1., 1.);
    this->displayGrid = true;
    this->displayMesh = true;
    this->previewCursorInPlanarView = false;
    this->multiGridRendering = false;
    this->drawOnlyBoundaries = true;
    this->blendFirstPass = 1.;
    this->sortingRendering = false;
}

Scene::~Scene(void) {
}

void Scene::initGl(QOpenGLContext* _context) {
    // Check if the scene has been initialized, share contexts if it has been :
    // this->h = 1024;
    // this->w = 768;
    this->h = 2024;
    this->w = 1468;
    //if (this->isInitialized == true) {
    //    if (this->context != nullptr && (_context != 0 && _context != nullptr)) {
    //        _context->setShareContext(this->context);
    //        if (_context->create() == false) {
    //            // throw std::runtime_error("Couldn't re-create context with shared context added\n");
    //            std::cerr << "Couldn't re-create context with shared context added\n";
    //        } else {
    //            std::cerr << "init() : Switching to context " << _context << '\n';
    //        }
    //    }
    //    return;
    //} else {
        // If the scene had not yet been initialized, it is now :

        // Set the context for later viewers that want to connect to the scene :
        if (_context == 0) {
            throw std::runtime_error("Warning : this->context() returned 0 or nullptr !");
        }
        if (_context == nullptr) {
            std::cerr << "Warning : Initializing a scene without a valid OpenGL context !" << '\n';
        }
        this->context = _context;

        // Get OpenGL functions from the currently bound context :
        if (this->initializeOpenGLFunctions() == false) {
            throw std::runtime_error("Could not initialize OpenGL functions.");
        }
    //}
    this->sceneGL.initGl(_context);

    auto maj = this->context->format().majorVersion();
    auto min = this->context->format().minorVersion();
    std::cerr << "OpenGL version " << maj << '.' << min << '\n';

    std::cerr << "OpenGL vendor string : " << glGetString(GL_VENDOR) << '\n';
    std::cerr << "OpenGL renderer string : " << glGetString(GL_RENDERER) << '\n';

    // Create the debug logger and connect its signals :
        //this->setupGLOutput();

    // The default parameters have already been set in the constructor. We
    // need to initialize the OpenGL objects now. Shaders, VAOs, VBOs.

    // Initialize limits
    this->maximumTextureSize = 0;

    glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &maximumTextureSize);
    std::cerr << "Info : max texture size was set to : " << this->maximumTextureSize << "\n";

    this->generateColorScales();

    this->shaderCompiler = std::make_unique<ShaderCompiler>(this);

    // Compile the shaders :
    this->recompileShaders(false);

    this->createBuffers();

    // Generate visibility array :
    this->tex_ColorScaleGrid = 0;

    // Generate controller positions
    //this->initGL(this->get_context());
    this->sceneGL.initGl(this->context);
    this->gridToDraw = -1;
    //this->prepareSphere();

    //this->surfaceMesh = nullptr;
    //this->drawableMesh = nullptr;
    this->glSelection->prepare();
}

void Scene::generateColorScales() {
    TextureUpload colorScaleUploadParameters;

    std::size_t textureSize = this->maximumTextureSize / 2u;
    float textureSize_f		= static_cast<float>(this->maximumTextureSize / 2u);

    std::vector<glm::vec3> colorScaleData_greyscale(textureSize);
    std::vector<glm::vec3> colorScaleData_hsv2rgb(textureSize);

    // Generate the greyscale :
    for (std::size_t i = 0; i < textureSize; ++i) {
        float intensity = static_cast<float>(i) / textureSize_f;
        glm::vec3 currentGreyscale(intensity, intensity, intensity);
        colorScaleData_greyscale[i] = currentGreyscale;
    }

    std::cerr << "Generated data for the greyscale color scale\n";

    // Generate the HSV2RGB :
    for (std::size_t i = 0; i < textureSize; ++i) {
        glm::vec3 hsv						= glm::vec3(float(i) / textureSize_f, 1., 1.);
        hsv.x								= glm::mod(100.0 + hsv.x, 1.0);	   // Ensure [0,1[
        float HueSlice						= 6.0 * hsv.x;	  // In [0,6[
        float HueSliceInteger				= floor(HueSlice);
        float HueSliceInterpolant			= HueSlice - HueSliceInteger;	 // In [0,1[ for each hue slice
        glm::vec3 TempRGB					= glm::vec3(hsv.z * (1.0 - hsv.y), hsv.z * (1.0 - hsv.y * HueSliceInterpolant), hsv.z * (1.0 - hsv.y * (1.0 - HueSliceInterpolant)));
        float IsOddSlice					= glm::mod(HueSliceInteger, 2.0f);	  // 0 if even (slices 0, 2, 4), 1 if odd (slices 1, 3, 5)
        float ThreeSliceSelector			= 0.5 * (HueSliceInteger - IsOddSlice);	   // (0, 1, 2) corresponding to slices (0, 2, 4) and (1, 3, 5)
        glm::vec3 ScrollingRGBForEvenSlices = glm::vec3(hsv.z, TempRGB.z, TempRGB.x);	 // (V, Temp Blue, Temp Red) for even slices (0, 2, 4)
        glm::vec3 ScrollingRGBForOddSlices	= glm::vec3(TempRGB.y, hsv.z, TempRGB.x);	 // (Temp Green, V, Temp Red) for odd slices (1, 3, 5)
        glm::vec3 ScrollingRGB				= mix(ScrollingRGBForEvenSlices, ScrollingRGBForOddSlices, IsOddSlice);
        float IsNotFirstSlice				= glm::clamp(ThreeSliceSelector, 0.0f, 1.0f);	 // 1 if NOT the first slice (true for slices 1 and 2)
        float IsNotSecondSlice				= glm::clamp(ThreeSliceSelector - 1.0f, 0.0f, 1.f);	   // 1 if NOT the first or second slice (true only for slice 2)
        colorScaleData_hsv2rgb[i]			= glm::vec4(glm::mix(glm::vec3(ScrollingRGB), glm::mix(glm::vec3(ScrollingRGB.z, ScrollingRGB.x, ScrollingRGB.y), glm::vec3(ScrollingRGB.y, ScrollingRGB.z, ScrollingRGB.x), IsNotSecondSlice), IsNotFirstSlice), 1.f);	   // Make the RGB rotate right depending on final slice index
    }

    std::cerr << "Generated data for the HSV2RGB color scale" << '\n';

    colorScaleUploadParameters.minmag.x	 = GL_LINEAR;
    colorScaleUploadParameters.minmag.y	 = GL_LINEAR;
    colorScaleUploadParameters.lod.y	 = -1000.f;
    colorScaleUploadParameters.wrap.x	 = GL_CLAMP_TO_EDGE;
    colorScaleUploadParameters.wrap.y	 = GL_CLAMP_TO_EDGE;
    colorScaleUploadParameters.wrap.z	 = GL_CLAMP_TO_EDGE;
    colorScaleUploadParameters.swizzle.r = GL_RED;
    colorScaleUploadParameters.swizzle.g = GL_GREEN;
    colorScaleUploadParameters.swizzle.b = GL_BLUE;
    colorScaleUploadParameters.swizzle.a = GL_ONE;

    colorScaleUploadParameters.level		  = 0;
    colorScaleUploadParameters.internalFormat = GL_RGB;
    colorScaleUploadParameters.size.x		  = textureSize;
    colorScaleUploadParameters.size.y		  = 1;
    colorScaleUploadParameters.size.z		  = 1;
    colorScaleUploadParameters.format		  = GL_RGB;
    colorScaleUploadParameters.type			  = GL_FLOAT;
    colorScaleUploadParameters.data			  = colorScaleData_greyscale.data();

    glDeleteTextures(1, &this->tex_colorScale_greyscale);
    this->tex_colorScale_greyscale = this->uploadTexture1D(colorScaleUploadParameters);

    colorScaleUploadParameters.data	   = colorScaleData_hsv2rgb.data();
    glDeleteTextures(1, &this->tex_colorScale_hsv2rgb);
    this->tex_colorScale_hsv2rgb = this->uploadTexture1D(colorScaleUploadParameters);
}

void Scene::addStatusBar(QStatusBar* _s) {
    if (this->programStatusBar != nullptr) {
        return;
    }

    this->programStatusBar = _s;
}

void Scene::createBuffers() {
    /// @brief Create a vertex array, bind it and see if it has been succesfully created server-side.
    auto createVAO = [&, this](std::string name) -> GLuint {
        GLuint buf = 0;
        this->glGenVertexArrays(1, &buf);
        this->glBindVertexArray(buf);
        if (this->glIsVertexArray(buf) == GL_FALSE) {
            std::cerr << "[ERROR][" << __FILE__ << ":" << __LINE__ << "] : Could not create VAO object " << name << '\n';
        }
        return buf;
    };

    /// @brief Create a buffer, bind it and see if it has been succesfully created server-side.
    auto createVBO = [&, this](GLenum bufType, std::string name) -> GLuint {
        GLuint buf = 0;
        this->glGenBuffers(1, &buf);
        this->glBindBuffer(bufType, buf);
        if (this->glIsBuffer(buf) == GL_FALSE) {
            std::cerr << "[ERROR][" << __FILE__ << ":" << __LINE__ << "] : Could not create buffer object " << name << '\n';
        }
        return buf;
    };

    // For the default VAO :
    this->vao					   = createVAO("vaoHandle");
    this->vbo_VertPos			   = createVBO(GL_ARRAY_BUFFER, "vboHandle_VertPos");
    this->vbo_VertNorm		       = createVBO(GL_ARRAY_BUFFER, "vboHandle_VertNorm");
    this->vbo_VertTex			   = createVBO(GL_ARRAY_BUFFER, "vboHandle_VertTex");
    this->vbo_Element			   = createVBO(GL_ELEMENT_ARRAY_BUFFER, "vboHandle_Element");

    // For the texture3D visualization method :
    this->vao_VolumetricBuffers  = createVAO("vaoHandle_VolumetricBuffers");
    this->vbo_Texture3D_VertPos  = createVBO(GL_ARRAY_BUFFER, "vboHandle_Texture3D_VertPos");
    this->vbo_Texture3D_VertNorm = createVBO(GL_ARRAY_BUFFER, "vboHandle_Texture3D_VertNorm");
    this->vbo_Texture3D_VertTex  = createVBO(GL_ARRAY_BUFFER, "vboHandle_Texture3D_VertTex");
    this->vbo_Texture3D_VertIdx  = createVBO(GL_ELEMENT_ARRAY_BUFFER, "vboHandle_Texture3D_VertIdx");

    // For the bounding boxes we have to create/show :
    this->vao_boundingBox			= createVAO("vaoHandle_boundingBox");
    this->vbo_boundingBoxVertices = createVBO(GL_ARRAY_BUFFER, "vboHandle_boundingBoxVertices");
    this->vbo_boundingBoxIndices	= createVBO(GL_ELEMENT_ARRAY_BUFFER, "vboHandle_boundingBoxIndices");

    this->glSelection->setVao(createVAO("vaoHandle_Selection"));
    this->glSelection->setVboVertices(createVBO(GL_ARRAY_BUFFER, "vboHandle_SelectionVertices"));
    this->glSelection->setVboIndices(createVBO(GL_ELEMENT_ARRAY_BUFFER, "vboHandle_SelectionIndices"));

    glGenTextures(1, &this->dualRenderingTexture);

    glBindTexture(GL_TEXTURE_2D, this->dualRenderingTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, h, w, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glGenTextures(1, &this->frameDepthBuffer);
    glBindTexture(GL_TEXTURE_2D, this->frameDepthBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0,GL_DEPTH_COMPONENT32, h, w, 0,GL_DEPTH_COMPONENT, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, this->frameDepthBuffer, 0);

    // Set "renderedTexture" as our colour attachement #0
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, this->dualRenderingTexture, 0);

    // Set the list of draw buffers.
    GLenum DrawBuffers[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, DrawBuffers); // "1" is the size of DrawBuffers

    // Always check that our framebuffer is ok
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "WARNING: framebuffer doesn't work !!" << std::endl;
    else
        std::cout << "Framebuffer works perfectly :) !!" << std::endl;

    static const GLfloat vertices[] = {
        -0.5f, -0.5f, 0.0f,
        0.5f, -0.5f, 0.0f,
        0.0f,  0.5f, 0.0f
    };

    glGenVertexArrays(1, &quad_VertexArrayID);
    glGenBuffers(1, &quad_vertexbuffer);

    glBindVertexArray(quad_VertexArrayID);
    glBindBuffer(GL_ARRAY_BUFFER, quad_vertexbuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    return;
}

std::pair<uint16_t, uint16_t> Scene::sendGridValuesToGPU(int gridIdx) {

    glm::vec<4, std::size_t, glm::defaultp> dimensions{this->grids[gridIdx]->grid->getResolution(), 2};
    TextureUpload _gridTex{};
    _gridTex.minmag.x  = GL_NEAREST;
    _gridTex.minmag.y  = GL_NEAREST;
    _gridTex.lod.y	   = -1000.f;
    _gridTex.wrap.x	   = GL_CLAMP_TO_EDGE;
    _gridTex.wrap.y	   = GL_CLAMP_TO_EDGE;
    _gridTex.wrap.z	   = GL_CLAMP_TO_EDGE;
    _gridTex.swizzle.r = GL_RED;
    if (dimensions.a > 1) {
        _gridTex.swizzle.g = GL_GREEN;
    } else {
        _gridTex.swizzle.g = GL_ZERO;
    }
    if (dimensions.a > 2) {
        _gridTex.swizzle.b = GL_BLUE;
    } else {
        _gridTex.swizzle.b = GL_ZERO;
    }
    if (dimensions.a > 3) {
        _gridTex.swizzle.a = GL_ALPHA;
    } else {
        _gridTex.swizzle.a = GL_ONE;
    }
    _gridTex.alignment.x = 1;
    _gridTex.alignment.y = 2;
    switch (dimensions.a) {
        case 1:
            _gridTex.format			= GL_RED_INTEGER;
            _gridTex.internalFormat = GL_R16UI;
            break;
        case 2:
            _gridTex.format			= GL_RG_INTEGER;
            _gridTex.internalFormat = GL_RG16UI;
            break;
        case 3:
            _gridTex.format			= GL_RGB_INTEGER;
            _gridTex.internalFormat = GL_RGB16UI;
            break;
        case 4:
            _gridTex.format			= GL_RGBA_INTEGER;
            _gridTex.internalFormat = GL_RGBA16UI;
            break;
    }
    _gridTex.type = GL_UNSIGNED_SHORT;
    std::cerr << "Made the upload texture struct.\n";

    _gridTex.size.x = dimensions.x;
    _gridTex.size.y = dimensions.y;
    _gridTex.size.z = dimensions.z;

    std::vector<std::uint16_t> slices;
    glDeleteTextures(1, &this->grids[gridIdx]->gridTexture);
    this->grids[gridIdx]->gridTexture = this->newAPI_uploadTexture3D_allocateonly(_gridTex);

    int nbSlice = this->grids[gridIdx]->grid->getResolution()[2];

    //TODO: this computation do not belong here
    uint16_t max = std::numeric_limits<uint16_t>::min();
    uint16_t min = std::numeric_limits<uint16_t>::max();

    dimensions[0] = dimensions[0] * dimensions[3];// Because we have "a" value

    bool addArticialBoundaries = false;

    int sliceI = 0;
    for (std::size_t s = 0; s < nbSlice; ++s) {
        this->grids[gridIdx]->grid->getGridSlice(s, slices, dimensions.a);
        if(addArticialBoundaries) {
            if(s == 0 || s == nbSlice-1){
                std::fill(slices.begin(), slices.end(), 0);
            } else {
                for(int i = 0; i < dimensions.x; ++i) {
                    slices[i] = 0;
                    slices[i+(dimensions.x*(dimensions.y-1))] = 0;
                }
                for(int i = 0; i < dimensions.y; ++i) {
                    slices[i*dimensions.x] = 0;
                    slices[i*dimensions.x+1] = 0;
                    slices[i*dimensions.x+(dimensions.x-1)] = 0;
                    slices[i*dimensions.x+(dimensions.x-2)] = 0;
                }
            }
        }
        this->newAPI_uploadTexture3D(this->grids[gridIdx]->gridTexture, _gridTex, sliceI, slices);

        max = std::max(max, *std::max_element(slices.begin(), slices.end()));
        //min = std::min(min, *std::min_element(slices.begin(), slices.end()));
        for(uint16_t& val : slices) {
            if(val > 0 && val < min)
                min = val;
        }
        slices.clear();
        sliceI++;
    }
    this->shouldUpdateUBOData = true;
    return std::make_pair(min, max);
}

void Scene::addGrid() {

    GridGLView::Ptr gridView = this->grids.back();

    glm::vec<4, std::size_t, glm::defaultp> dimensions{gridView->grid->getResolution(), 2};

    std::pair<uint16_t, uint16_t> min_max = sendGridValuesToGPU(this->grids.size() -1);

    uint16_t min = min_max.first;
    uint16_t max = min_max.second;
    gridView->minValue = min;
    gridView->maxValue = max;

    if(this->gridToDraw == 0) {
        QColor r = Qt::GlobalColor::red;
        QColor b = Qt::GlobalColor::blue;
        gridView->color_0 = glm::vec3(r.redF(), r.greenF(), r.blueF());
        gridView->color_1 = glm::vec3(b.redF(), b.greenF(), b.blueF());
    } else {
        QColor d = Qt::GlobalColor::darkCyan;
        QColor y = Qt::GlobalColor::yellow;
        gridView->color_0 = glm::vec3(d.redF(), d.greenF(), d.blueF());
        gridView->color_1 = glm::vec3(y.redF(), y.greenF(), y.blueF());
    }

    gridView->colorChannelAttributes[0].setMaxVisible(max);
    gridView->colorChannelAttributes[0].setMaxColorScale(max);
    gridView->colorChannelAttributes[1].setMaxVisible(max);
    gridView->colorChannelAttributes[1].setMaxColorScale(max);
    gridView->colorChannelAttributes[2].setMaxVisible(max);
    gridView->colorChannelAttributes[2].setMaxColorScale(max);

    if(this->gridToDraw == 1) {
        this->controlPanel->setMaxTexValAlternate(max);
        this->controlPanel->updateMaxValueAlternate(max);
        this->slotSetMaxColorValueAlternate(max);
    } else {
        this->controlPanel->setMaxTexVal(max);
        this->controlPanel->updateMaxValue(max);
        this->slotSetMaxColorValue(max);
    }
    this->setColorChannel(ColorChannel::RedOnly);
    this->setColorFunction_r(ColorFunction::ColorMagnitude);
    this->setColorFunction_g(ColorFunction::ColorMagnitude);

    gridView->boundingBoxColor = glm::vec3(.4, .6, .3);	   // olive-colored by default
    gridView->nbChannels	   = 2;	   // loaded 2 channels in the image

    // Create the uniform buffer :
    auto mainColorChannel				= gridView->mainColorChannelAttributes();
    gridView->uboHandle_colorAttributes = this->createUniformBuffer(4 * sizeof(ColorChannelAttributes_GL), GL_STATIC_DRAW);
    this->setUniformBufferData(gridView->uboHandle_colorAttributes, 0, 32, &mainColorChannel);
    this->setUniformBufferData(gridView->uboHandle_colorAttributes, 32, 32, &gridView->colorChannelAttributes[0]);
    this->setUniformBufferData(gridView->uboHandle_colorAttributes, 64, 32, &gridView->colorChannelAttributes[1]);
    this->setUniformBufferData(gridView->uboHandle_colorAttributes, 96, 32, &gridView->colorChannelAttributes[2]);

    //Send grid texture
    this->sendTetmeshToGPU(this->gridToDraw, InfoToSend(InfoToSend::VERTICES | InfoToSend::NORMALS | InfoToSend::TEXCOORD | InfoToSend::NEIGHBORS));
    this->tex3D_buildBuffers(gridView->volumetricMesh);

    this->updateBoundingBox();
}

void Scene::updateBoundingBox(void) {
    std::cerr << "Error: updateBoundingBox brocken due to new API update" << std::endl;
    this->sceneBB	  = Image::bbox_t();
    this->sceneDataBB = Image::bbox_t();

    for (std::size_t i = 0; i < this->grids.size(); ++i) {
        const Grid * _g = this->grids[i]->grid;
        Image::bbox_t box		  = _g->getBoundingBox();
        Image::bbox_t dbox		  = _g->getBoundingBox();
        this->sceneBB.addPoints(box.getAllCorners());
        this->sceneDataBB.addPoints(dbox.getAllCorners());
    }

    // Update light positions :
    auto corners = this->sceneBB.getAllCorners();
    for (std::size_t i = 0; i < corners.size(); ++i) {
        this->lightPositions[i] = glm::convert_to<float>(corners[i]);
    }

    return;
}

void Scene::recompileShaders(bool verbose) {
    GLuint newProgram			 = this->compileShaders("../shaders/voxelgrid.vert", "../shaders/voxelgrid.geom", "../shaders/voxelgrid.frag", verbose);
    GLuint newPlaneProgram		 = this->compileShaders("../shaders/plane.vert", "", "../shaders/plane.frag", verbose);
    GLuint newPlaneViewerProgram = this->compileShaders("../shaders/texture_explorer.vert", "", "../shaders/texture_explorer.frag", verbose);
    GLuint newVolumetricProgram	 = this->compileShaders("../shaders/transfer_mesh.vert", "../shaders/transfer_mesh.geom", "../shaders/transfer_mesh.frag", verbose);
    GLuint newBoundingBoxProgram = this->compileShaders("../shaders/bounding_box.vert", "", "../shaders/bounding_box.frag", verbose);
    GLuint newSphereProgram		 = this->compileShaders("../shaders/sphere.vert", "", "../shaders/sphere.frag", true);
    GLuint newSelectionProgram	 = this->compileShaders("../shaders/selection.vert", "", "../shaders/selection.frag", true);
    GLuint newDoublePassProgram  = this->compileShaders("../shaders/double_pass.vert", "", "../shaders/double_pass.frag", true);


    if (newProgram) {
        glDeleteProgram(this->program_projectedTex);
        this->program_projectedTex = newProgram;
    }
    if (newPlaneProgram) {
        glDeleteProgram(this->program_Plane3D);
        this->program_Plane3D = newPlaneProgram;
    }
    if (newPlaneViewerProgram) {
        glDeleteProgram(this->program_PlaneViewer);
        this->program_PlaneViewer = newPlaneViewerProgram;
    }
    if (newVolumetricProgram) {
        glDeleteProgram(this->program_VolumetricViewer);
        this->program_VolumetricViewer = newVolumetricProgram;
    }
    if (newBoundingBoxProgram) {
        glDeleteProgram(this->program_BoundingBox);
        this->program_BoundingBox = newBoundingBoxProgram;
    }
    if (newSelectionProgram) {
        glDeleteProgram(this->glSelection->getProgram());
        this->glSelection->setProgram(newSelectionProgram);
    }
    if (newDoublePassProgram) {
        glDeleteProgram(this->program_doublePass);
        this->program_doublePass = newDoublePassProgram;
    }
}

GLuint Scene::compileShaders(std::string _vPath, std::string _gPath, std::string _fPath, bool verbose) {
    glUseProgram(0);
    this->shaderCompiler->reset();
    this->shaderCompiler->pragmaReplacement_file("include_color_shader", "../shaders/colorize_new_flow.glsl");
    this->shaderCompiler->vertexShader_file(_vPath).geometryShader_file(_gPath).fragmentShader_file(_fPath);
    if (this->shaderCompiler->compileShaders()) {
        return this->shaderCompiler->programName();
    }
    std::cerr << this->shaderCompiler->errorString() << '\n';
    return 0;
}

GLuint Scene::uploadTexture1D(const TextureUpload& tex) {
    if (this->context != nullptr) {
        if (this->context->isValid() == false) {
            throw std::runtime_error("No associated valid context");
        }
    } else {
        throw std::runtime_error("nullptr as context");
    }

    glEnable(GL_TEXTURE_1D);

    GLuint texHandle = 0;
    glGenTextures(1, &texHandle);
    glBindTexture(GL_TEXTURE_1D, texHandle);

    // Min and mag filters :
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, tex.minmag.x);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, tex.minmag.y);

    // Set the min and max LOD values :
    glTexParameterf(GL_TEXTURE_1D, GL_TEXTURE_MIN_LOD, tex.lod.x);
    glTexParameterf(GL_TEXTURE_1D, GL_TEXTURE_MAX_LOD, tex.lod.y);

    // Set the wrap parameters :
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, tex.wrap.x);

    // Set the swizzle the user wants :
    glTexParameteriv(GL_TEXTURE_1D, GL_TEXTURE_SWIZZLE_RGBA, glm::value_ptr(tex.swizzle));

    // Set the pixel alignment :
    glPixelStorei(GL_PACK_ALIGNMENT, tex.alignment.x);
    glPixelStorei(GL_UNPACK_ALIGNMENT, tex.alignment.y);

    glTexImage1D(GL_TEXTURE_1D,	   // GLenum : Target
      static_cast<GLint>(tex.level),	// GLint  : Level of detail of the current texture (0 = original)
      tex.internalFormat,	 // GLint  : Number of color components in the picture.
      tex.size.x,	 // GLsizei: Image width
      static_cast<GLint>(0),	// GLint  : Border. This value MUST be 0.
      tex.format,	 // GLenum : Format of the pixel data
      tex.type,	   // GLenum : Type (the data type as in uchar, uint, float ...)
      tex.data	  // void*  : Data to load into the buffer
    );

    return texHandle;
}

GLuint Scene::uploadTexture2D(const TextureUpload& tex) {
    if (this->context != nullptr) {
        if (this->context->isValid() == false) {
            throw std::runtime_error("No associated valid context");
        }
    } else {
        throw std::runtime_error("nullptr as context");
    }

    glEnable(GL_TEXTURE_2D);

    GLuint texHandle = 0;
    glGenTextures(1, &texHandle);
    glBindTexture(GL_TEXTURE_2D, texHandle);

    // Min and mag filters :
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, tex.minmag.x);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, tex.minmag.y);

    // Set the min and max LOD values :
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, tex.lod.x);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, tex.lod.y);

    // Set the wrap parameters :
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, tex.wrap.x);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, tex.wrap.y);

    // Set the swizzle the user wants :
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, glm::value_ptr(tex.swizzle));

    // Set the pixel alignment :
    glPixelStorei(GL_PACK_ALIGNMENT, tex.alignment.x);
    glPixelStorei(GL_UNPACK_ALIGNMENT, tex.alignment.y);

    glTexImage2D(GL_TEXTURE_2D,	   // GLenum : Target
      static_cast<GLint>(tex.level),	// GLint  : Level of detail of the current texture (0 = original)
      tex.internalFormat,	 // GLint  : Number of color components in the picture. Here grayscale so GL_RED
      tex.size.x,	 // GLsizei: Image width
      tex.size.y,	 // GLsizei: Image height
      static_cast<GLint>(0),	// GLint  : Border. This value MUST be 0.
      tex.format,	 // GLenum : Format of the pixel data
      tex.type,	   // GLenum : Type (the data type as in uchar, uint, float ...)
      tex.data	  // void*  : Data to load into the buffer
    );

    return texHandle;
}

GLuint Scene::uploadTexture3D(const TextureUpload& tex) {
    if (this->context != nullptr) {
        if (this->context->isValid() == false) {
            throw std::runtime_error("No associated valid context");
        }
    } else {
        throw std::runtime_error("nullptr as context");
    }

    glEnable(GL_TEXTURE_3D);

    GLuint texHandle = 0;
    glGenTextures(1, &texHandle);
    glBindTexture(GL_TEXTURE_3D, texHandle);

    // Min and mag filters :
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, tex.minmag.x);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, tex.minmag.y);

    // Set the min and max LOD values :
    glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_MIN_LOD, tex.lod.x);
    glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_MAX_LOD, tex.lod.y);

    // Set the wrap parameters :
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, tex.wrap.x);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, tex.wrap.y);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, tex.wrap.z);

    // Set the swizzle the user wants :
    glTexParameteriv(GL_TEXTURE_3D, GL_TEXTURE_SWIZZLE_RGBA, glm::value_ptr(tex.swizzle));

    // Set the pixel alignment :
    glPixelStorei(GL_PACK_ALIGNMENT, tex.alignment.x);
    glPixelStorei(GL_UNPACK_ALIGNMENT, tex.alignment.y);

    glTexImage3D(GL_TEXTURE_3D,	   // GLenum : Target
      static_cast<GLint>(tex.level),	// GLint  : Level of detail of the current texture (0 = original)
      tex.internalFormat,	 // GLint  : Number of color components in the picture. Here grayscale so GL_RED
      tex.size.x,	 // GLsizei: Image width
      tex.size.y,	 // GLsizei: Image height
      tex.size.z,	 // GLsizei: Image depth (number of layers)
      static_cast<GLint>(0),	// GLint  : Border. This value MUST be 0.
      tex.format,	 // GLenum : Format of the pixel data
      tex.type,	   // GLenum : Type (the data type as in uchar, uint, float ...)
      tex.data	  // void*  : Data to load into the buffer
    );

    return texHandle;
}

GLuint Scene::newAPI_uploadTexture3D_allocateonly(const TextureUpload& tex) {
    if (this->context != nullptr) {
        if (this->context->isValid() == false) {
            throw std::runtime_error("No associated valid context");
        }
    } else {
        throw std::runtime_error("nullptr as context");
    }

    glEnable(GL_TEXTURE_3D);

    GLuint texHandle = 0;
    glGenTextures(1, &texHandle);
    glBindTexture(GL_TEXTURE_3D, texHandle);

    // Min and mag filters :
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, tex.minmag.x);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, tex.minmag.y);

    // Set the min and max LOD values :
    glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_MIN_LOD, tex.lod.x);
    glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_MAX_LOD, tex.lod.y);

    // Set the wrap parameters :
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, tex.wrap.x);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, tex.wrap.y);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, tex.wrap.z);

    // Set the swizzle the user wants :
    glTexParameteriv(GL_TEXTURE_3D, GL_TEXTURE_SWIZZLE_RGBA, glm::value_ptr(tex.swizzle));

    // Set the pixel alignment :
    glPixelStorei(GL_PACK_ALIGNMENT, tex.alignment.x);
    glPixelStorei(GL_UNPACK_ALIGNMENT, tex.alignment.y);

    glTexImage3D(GL_TEXTURE_3D,	   // GLenum : Target
      static_cast<GLint>(tex.level),	// GLint  : Level of detail of the current texture (0 = original)
      tex.internalFormat,	 // GLint  : Number of color components in the picture. Here grayscale so GL_RED
      tex.size.x,	 // GLsizei: Image width
      tex.size.y,	 // GLsizei: Image height
      tex.size.z,	 // GLsizei: Image depth (number of layers)
      static_cast<GLint>(0),	// GLint  : Border. This value MUST be 0.
      tex.format,	 // GLenum : Format of the pixel data
      tex.type,	   // GLenum : Type (the data type as in uchar, uint, float ...)
      nullptr	 // no data here !
    );

    return texHandle;
}

GLuint Scene::newAPI_uploadTexture3D(const GLuint texHandle, const TextureUpload& tex, std::size_t s, std::vector<std::uint16_t>& data) {
    if (this->context != nullptr) {
        if (this->context->isValid() == false) {
            throw std::runtime_error("No associated valid context");
        }
    } else {
        throw std::runtime_error("nullptr as context");
    }

    glEnable(GL_TEXTURE_3D);
    glBindTexture(GL_TEXTURE_3D, texHandle);

    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, s, tex.size.x, tex.size.y, 1, tex.format, tex.type, data.data());

    return texHandle;
}

//void Scene::drawGridVolumetricView(GLfloat* mvMat, GLfloat* pMat, glm::vec3 camPos, const GridGLView::Ptr& grid) {
void Scene::drawGrid(GLfloat* mvMat, GLfloat* pMat, glm::vec3 camPos, const GridGLView::Ptr& grid, bool inFrame) {

    if (grid->gridTexture > 0) {
        glUseProgram(this->program_VolumetricViewer);

        if(sortingRendering) {
            this->sendTetmeshToGPU(this->gridToDraw, InfoToSend(InfoToSend::VERTICES | InfoToSend::NORMALS), sortingRendering);
        }

        this->prepareUniformsGrid(mvMat, pMat, camPos, grid, !inFrame);

        this->tex3D_bindVAO();
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->vbo_Texture3D_VertIdx);

        //glDrawElementsInstanced(GL_TRIANGLES, 12, GL_UNSIGNED_SHORT, (void*) 0, grid->volumetricMesh.tetrahedraCount);

        if(inFrame && this->multiGridRendering) {
            //glBindTexture(GL_TEXTURE_2D, this->dualRenderingTexture);
            //glTexImage2D(GL_TEXTURE_2D, 0,GL_RGB, 1024, 768, 0,GL_RGB, GL_UNSIGNED_BYTE, 0);

            //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &this->defaultFBO);
            /***/
            glDeleteFramebuffers(1, &this->frameBuffer);
            glGenFramebuffers(1, &this->frameBuffer);
            glBindFramebuffer(GL_FRAMEBUFFER, this->frameBuffer);

            glDeleteTextures(1, &this->dualRenderingTexture);
            glGenTextures(1, &this->dualRenderingTexture);

            glBindTexture(GL_TEXTURE_2D, this->dualRenderingTexture);
            glTexImage2D(GL_TEXTURE_2D, 0,GL_RGB, h, w, 0,GL_RGB, GL_UNSIGNED_BYTE, 0);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

            glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, this->dualRenderingTexture, 0);

            glDeleteTextures(1, &this->frameDepthBuffer);
            glGenTextures(1, &this->frameDepthBuffer);
            glBindTexture(GL_TEXTURE_2D, this->frameDepthBuffer);
            glTexImage2D(GL_TEXTURE_2D, 0,GL_DEPTH_COMPONENT16, h, w, 0,GL_DEPTH_COMPONENT, GL_FLOAT, 0);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, this->frameDepthBuffer, 0);

            // Set the list of draw buffers.
            GLenum DrawBuffers[1] = {GL_COLOR_ATTACHMENT0};
            glDrawBuffers(1, DrawBuffers); // "1" is the size of DrawBuffers
            /***/
            glDrawElementsInstanced(GL_TRIANGLES, 12, GL_UNSIGNED_SHORT, (void*) 0, grid->volumetricMesh.tetrahedraCount);
            glBindFramebuffer(GL_FRAMEBUFFER, this->defaultFBO);
            //glDrawBuffer(0);
        } else {
            glDrawElementsInstanced(GL_TRIANGLES, 12, GL_UNSIGNED_SHORT, (void*) 0, grid->volumetricMesh.tetrahedraCount);
        }

        // Unbind program, buffers and VAO :
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
        glUseProgram(0);
    }

    // draw grid BB :
    // Deactivate for now because useless and create a bug with the normal display
    //this->drawBoundingBox(grid->grid->getBoundingBox(), grid->boundingBoxColor, mvMat, pMat);
}

glm::vec3 Scene::computePlanePositionsWithActivation() {
    glm::vec3 planePos = this->computePlanePositions();
    for(int i = 0; i < 3; ++i) {
        if(this->planeActivation[i] == 0.) {
            planePos[i] = -1000000.;
        }
    }
    return planePos;
}

glm::vec3 Scene::computePlanePositions() {
    //Image::bbox_t::vec position = this->sceneBB.getMin();
    if(this->getBaseMesh(this->activeMesh)) {
        // Bad tricks
        int idx = this->getGridIdx(this->activeMesh);
        Image::bbox_t::vec position = this->getBaseMesh(this->activeMesh)->bbMin;
        Image::bbox_t::vec diagonal = this->getBaseMesh(this->activeMesh)->getDimensions();
        glm::vec3 planePos			= (position + this->planeDisplacement * diagonal);
        if(idx == 0)
            planePos += glm::vec3(0.1, 0.1, 0.1);
        return planePos;
    } else {
        return glm::vec3(0., 0., 0.);
    }
}

GLuint Scene::createUniformBuffer(std::size_t size_bytes, GLenum draw_mode) {
    GLuint uniform_to_create = 0;
    glGenBuffers(1, &uniform_to_create);
    glBindBuffer(GL_UNIFORM_BUFFER, uniform_to_create);
    glBufferData(GL_UNIFORM_BUFFER, size_bytes, NULL, draw_mode);
    if (glIsBuffer(uniform_to_create) == GL_FALSE) {
        std::cerr << "Error : tried to create a " << size_bytes << " bytes buffer but failed.\n";
        uniform_to_create = 0;
    }
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    return uniform_to_create;
}

void Scene::setUniformBufferData(GLuint uniform_buffer, std::size_t begin_bytes, std::size_t size_bytes, GLvoid* data) {
    glBindBuffer(GL_UNIFORM_BUFFER, uniform_buffer);
    glBufferSubData(GL_UNIFORM_BUFFER, begin_bytes, size_bytes, data);
}

void Scene::printAllUniforms(GLuint _shader_program) {
    GLint i;
    GLint count;	// count of how many variables are there

    GLint size;	   // size of the variable
    GLenum type;	// type of the variable (float, vec3 or mat4, etc)

    const GLsizei bufSize = 64;	   // maximum name length
    GLchar name[bufSize];	 // variable name in GLSL
    GLsizei length;	   // name length

    glGetProgramiv(_shader_program, GL_ACTIVE_UNIFORMS, &count);
    fprintf(stderr, "Active Uniforms: %d\n", count);

    for (i = 0; i < count; i++)
    {
        glGetActiveUniform(_shader_program, (GLuint) i, bufSize, &length, &size, &type, name);
        fprintf(stderr, "\t- Uniform #%d : \"%s\"\n", i, name);
    }
}

//void Scene::prepareUniformsGridVolumetricView(GLfloat* mvMat, GLfloat* pMat, glm::vec3 camPos, const GridGLView::Ptr& _grid) {
void Scene::prepareUniformsGrid(GLfloat* mvMat, GLfloat* pMat, glm::vec3 camPos, const GridGLView::Ptr& _grid, bool drawFront) {

    // We assume the right program has been bound.

    /// @brief Shortcut for glGetUniform, since this can result in long lines.
    auto getUniform = [&](const char* name) -> GLint {
        GLint g = glGetUniformLocation(this->program_VolumetricViewer, name);
        return g;
    };

    // Texture handles :
    GLint location_vertices_translation	  = getUniform("vertices_translations");
    GLint location_normals_translation	  = getUniform("normals_translations");
    GLint location_visibility_texture	  = getUniform("visibility_texture");
    GLint location_texture_coordinates	  = getUniform("texture_coordinates");
    GLint location_neighbors			  = getUniform("neighbors");
    GLint location_Mask					  = getUniform("texData");
    GLint location_visibilityMap		  = getUniform("visiblity_map");
    GLint location_visibilityMapAlternate = getUniform("visiblity_map_alternate");
    GLint location_firstPass_texture      = getUniform("firstPass_texture");
    GLint location_firstPass_depthTexture = getUniform("firstPass_depthTexture");
    GLint location_valuesRangeToDisplay   = getUniform("valuesRangeToDisplay");
    GLint location_valuesRangeColorToDisplay   = getUniform("colorRangeToDisplay");


    std::size_t tex = 0;
    glActiveTexture(GL_TEXTURE0 + tex);
    glBindTexture(GL_TEXTURE_2D, _grid->volumetricMesh.vertexPositions);
    glUniform1i(location_vertices_translation, tex);
    tex++;

    glActiveTexture(GL_TEXTURE0 + tex);
    glBindTexture(GL_TEXTURE_2D, _grid->volumetricMesh.faceNormals);
    glUniform1i(location_normals_translation, tex);
    tex++;

    glActiveTexture(GL_TEXTURE0 + tex);
    glBindTexture(GL_TEXTURE_2D, _grid->volumetricMesh.visibilityMap);
    glUniform1i(location_visibility_texture, tex);
    tex++;

    glActiveTexture(GL_TEXTURE0 + tex);
    glBindTexture(GL_TEXTURE_2D, _grid->volumetricMesh.textureCoordinates);
    glUniform1i(location_texture_coordinates, tex);
    tex++;

    glActiveTexture(GL_TEXTURE0 + tex);
    glBindTexture(GL_TEXTURE_2D, _grid->volumetricMesh.neighborhood);
    glUniform1i(location_neighbors, tex);
    tex++;

    glActiveTexture(GL_TEXTURE0 + tex);
    glBindTexture(GL_TEXTURE_3D, _grid->gridTexture);
    glUniform1i(location_Mask, tex);
    tex++;

    glActiveTexture(GL_TEXTURE0 + tex);
    glBindTexture(GL_TEXTURE_2D, this->tex_ColorScaleGrid);
    glUniform1i(location_visibilityMap, tex);
    tex++;

    glActiveTexture(GL_TEXTURE0 + tex);
    glBindTexture(GL_TEXTURE_2D, this->tex_ColorScaleGridAlternate);
    glUniform1i(location_visibilityMapAlternate, tex);
    tex++;

    glActiveTexture(GL_TEXTURE0 + tex);
    glBindTexture(GL_TEXTURE_2D, this->dualRenderingTexture);
    glUniform1i(location_firstPass_texture, tex);
    tex++;

    glActiveTexture(GL_TEXTURE0 + tex);
    glBindTexture(GL_TEXTURE_2D, this->frameDepthBuffer);
    glUniform1i(location_firstPass_depthTexture, tex);
    tex++;

    glActiveTexture(GL_TEXTURE0 + tex);
    glBindTexture(GL_TEXTURE_1D, _grid->valuesRangeToDisplay);
    glUniform1i(location_valuesRangeToDisplay, tex);
    tex++;

    glActiveTexture(GL_TEXTURE0 + tex);
    glBindTexture(GL_TEXTURE_1D, _grid->valuesRangeColorToDisplay);
    glUniform1i(location_valuesRangeColorToDisplay, tex);
    tex++;

    GLint location_isFirstPass = getUniform("isFirstPass");
    float val = 1.;
    if(drawFront)
        val = 0.;
    glUniform1fv(location_isFirstPass, 1, &val);

    // Scalars :
    GLint location_voxelSize = getUniform("voxelSize");
    GLint location_gridSize	 = getUniform("gridSize");

    //glm::vec3 floatres = glm::convert_to<float>(_grid->grid->getDimensions());
    glm::vec3 floatres = glm::convert_to<float>(_grid->grid->sampler.getSamplerDimension());

    //glUniform3fv(location_voxelSize, 1, glm::value_ptr(_grid->grid->getWorldVoxelSize()));
    glUniform3fv(location_voxelSize, 1, glm::value_ptr(_grid->grid->getVoxelSize()));
    glUniform3fv(location_gridSize, 1, glm::value_ptr(floatres));

    // Vectors/arrays :
    GLint location_cam					  = getUniform("cam");
    GLint location_cut					  = getUniform("cut");
    GLint location_cutDirection			  = getUniform("cutDirection");
    GLint location_clipDistanceFromCamera = getUniform("clipDistanceFromCamera");
    GLint location_visuBBMin			  = getUniform("visuBBMin");
    GLint location_visuBBMax			  = getUniform("visuBBMax");
    GLint location_shouldUseBB			  = getUniform("shouldUseBB");
    GLint location_displayWireframe		  = getUniform("displayWireframe");
    GLint location_volumeEpsilon		  = getUniform("volumeEpsilon");
    GLint location_drawOnlyBoundaries    = getUniform("drawOnlyBoundaries");
    GLint location_blendFirstPass            = getUniform("blendFirstPass");

    glm::vec3 planePos	   = glm::vec3(0., 0., 0.);
    if(this->grids.size() > 0) {
        // Bad tricks
        Image::bbox_t::vec position = this->grids.back()->grid->bbMin;
        Image::bbox_t::vec diagonal = this->grids.back()->grid->getDimensions();
        planePos			= (position + this->planeDisplacement * diagonal);
        if(this->gridToDraw == 0)
            planePos += glm::vec3(0.1, 0.1, 0.1);
    }

    for(int i = 0; i < 3; ++i) {
        if(this->planeActivation[i] == 0.) {
            planePos[i] = -1000000.;
        }
    }

    Image::bbox_t::vec min = _grid->grid->bbMin;
    Image::bbox_t::vec max = _grid->grid->bbMax;

    glUniform3fv(location_cam, 1, glm::value_ptr(camPos));
    glUniform3fv(location_cut, 1, glm::value_ptr(planePos));
    glUniform3fv(location_cutDirection, 1, glm::value_ptr(this->planeDirection));
    glUniform1f(location_clipDistanceFromCamera, this->clipDistanceFromCamera);
    glUniform3fv(location_visuBBMin, 1, glm::value_ptr(min));
    glUniform3fv(location_visuBBMax, 1, glm::value_ptr(max));
    glUniform1ui(location_shouldUseBB, 0);
    glUniform1ui(location_displayWireframe, this->displayTetmesh);
    glUniform3fv(location_volumeEpsilon, 1, glm::value_ptr(_grid->defaultEpsilon));
    glUniform1f(location_blendFirstPass, this->blendFirstPass);
    glUniform1f(getUniform("maxValue"), _grid->maxValue);

    int drawOnly = 1;
    if(!this->drawOnlyBoundaries)
        drawOnly = 0;
    glUniform1i(location_drawOnlyBoundaries, drawOnly);

    // Matrices :
    GLint location_mMat = getUniform("mMat");
    GLint location_vMat = getUniform("vMat");
    GLint location_pMat = getUniform("pMat");

    glUniformMatrix4fv(location_mMat, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.f)));
    glUniformMatrix4fv(location_vMat, 1, GL_FALSE, mvMat);
    glUniformMatrix4fv(location_pMat, 1, GL_FALSE, pMat);

    // Light positions :
    GLint location_light0 = getUniform("lightPositions[0]");
    GLint location_light1 = getUniform("lightPositions[1]");
    GLint location_light2 = getUniform("lightPositions[2]");
    GLint location_light3 = getUniform("lightPositions[3]");
    GLint location_light4 = getUniform("lightPositions[4]");
    GLint location_light5 = getUniform("lightPositions[5]");
    GLint location_light6 = getUniform("lightPositions[6]");
    GLint location_light7 = getUniform("lightPositions[7]");

    glUniform3fv(location_light0, 1, glm::value_ptr(this->lightPositions[0]));
    glUniform3fv(location_light1, 1, glm::value_ptr(this->lightPositions[1]));
    glUniform3fv(location_light2, 1, glm::value_ptr(this->lightPositions[2]));
    glUniform3fv(location_light3, 1, glm::value_ptr(this->lightPositions[3]));
    glUniform3fv(location_light4, 1, glm::value_ptr(this->lightPositions[4]));
    glUniform3fv(location_light5, 1, glm::value_ptr(this->lightPositions[5]));
    glUniform3fv(location_light6, 1, glm::value_ptr(this->lightPositions[6]));
    glUniform3fv(location_light7, 1, glm::value_ptr(this->lightPositions[7]));

    // Color and shading parameters :
    GLint location_specRef	  = getUniform("specRef");
    GLint location_shininess  = getUniform("shininess");
    GLint location_diffuseRef = getUniform("diffuseRef");
    glUniform1f(location_specRef, .8f);
    glUniform1f(location_shininess, .8f);
    glUniform1f(location_diffuseRef, .8f);

    // User-defined colors :
    GLint location_color0	 = getUniform("color0");
    GLint location_color1	 = getUniform("color1");
    GLint location_color0Alt = getUniform("color0Alternate");
    GLint location_color1Alt = getUniform("color1Alternate");

    glUniform3fv(location_color0, 1, glm::value_ptr(this->color0));
    glUniform3fv(location_color1, 1, glm::value_ptr(this->color1));
    glUniform3fv(location_color0Alt, 1, glm::value_ptr(this->color0_second));
    glUniform3fv(location_color1Alt, 1, glm::value_ptr(this->color1_second));

    GLint location_colorScales0 = getUniform("colorScales[0]");
    GLint location_colorScales1 = getUniform("colorScales[1]");
    GLint location_colorScales2 = getUniform("colorScales[2]");
    GLint location_colorScales3 = getUniform("colorScales[3]");

    glActiveTexture(GL_TEXTURE0 + tex);
    glBindTexture(GL_TEXTURE_1D, this->tex_colorScale_greyscale);
    glUniform1i(location_colorScales0, tex);
    tex++;

    glActiveTexture(GL_TEXTURE0 + tex);
    glBindTexture(GL_TEXTURE_1D, this->tex_colorScale_hsv2rgb);
    glUniform1i(location_colorScales1, tex);
    tex++;

    GLuint colorScale = this->tex_colorScale_user0;
    if(this->gridToDraw > 0)
        colorScale = this->tex_colorScale_user1;

    glActiveTexture(GL_TEXTURE0 + tex);
    glBindTexture(GL_TEXTURE_1D, colorScale);
    glUniform1i(location_colorScales2, tex);
    tex++;

    glActiveTexture(GL_TEXTURE0 + tex);
    glBindTexture(GL_TEXTURE_1D, colorScale);
    glUniform1i(location_colorScales3, tex);
    tex++;

    const GLchar uniform_block_name[] = "ColorBlock";
    GLuint colorBlock_index			  = glGetUniformBlockIndex(this->program_projectedTex, uniform_block_name);
    glUniformBlockBinding(this->program_projectedTex, colorBlock_index, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, _grid->uboHandle_colorAttributes);
}

void Scene::drawScene(GLfloat* mvMat, GLfloat* pMat, glm::vec3 camPos, bool showTexOnPlane) {
    this->cameraPosition = camPos;
    if (this->shouldUpdateUserColorScales) {
        this->newSHADERS_updateUserColorScales();
    }
    if (this->shouldUpdateUBOData) {
        this->newSHADERS_updateUBOData();
    }

    this->setLightPosition(camPos);

    glEnable(GL_DEPTH_TEST);
    glEnablei(GL_BLEND, 0);
    glEnable(GL_TEXTURE_3D);
    //glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glm::mat4 transfoMat = glm::mat4(1.f);
    /* Manipulator drawing  */

    glm::mat4 mMat(1.0f);
    //this->draw(mvMat, pMat, glm::value_ptr(mMat), this->computePlanePositions());
    //this->draw(mvMat, pMat, glm::value_ptr(mMat), this->computePlanePositions());
    if(this->meshManipulator)
        this->meshManipulator->draw();

    for(int i = 0; i < this->graph_meshes.size(); ++i) {
        glm::vec3 planePos	   = this->computePlanePositions();
        for(int i = 0; i < 3; ++i) {
            if(this->planeActivation[i] == 0.) {
                planePos[i] = -1000000.;
            }
        }
        this->graph_meshes[i].first->draw(pMat, mvMat, glm::value_ptr(mMat), planePos);
    }

    /***********************/


    if(this->displayGrid) {
        if(this->multiGridRendering) {
            if(this->grids.size() > 0) {
                int originalGridToDraw = this->gridToDraw;
                for(auto i : this->gridsToDraw) {
                    if(i >= 0 && i < this->grids.size()) {
                        this->gridToDraw = i;
                        //this->drawGridVolumetricView(mvMat, pMat, camPos, this->grids[gridToDraw]);
                        this->drawGrid(mvMat, pMat, camPos, this->grids[gridToDraw], i == 0);
                    }
                }
                this->gridToDraw = originalGridToDraw;
            }
        } else {
            if(this->grids.size() > 0) {
                int originalGridToDraw = this->gridToDraw;
                //this->drawGridVolumetricView(mvMat, pMat, camPos, this->grids[gridToDraw]);
                for(auto i : this->gridsToDraw) {
                    if(i < this->grids.size()) {
                        this->gridToDraw = i;
                        this->drawGrid(mvMat, pMat, camPos, this->grids[i], false);
                    }
                }
                this->gridToDraw = originalGridToDraw;
            }
        }
    }

    if(this->displayMesh) {
        for(int i = 0; i < this->meshes.size(); ++i) {
            this->meshes[i].first->draw(pMat, mvMat, glm::vec4{camPos, 1.f}, this->computePlanePositionsWithActivation());
        }
    }

    //this->drawBoundingBox(this->sceneBB, glm::vec4(.5, .5, .0, 1.), mvMat, pMat);
    this->glSelection->draw(mvMat, pMat, glm::value_ptr(mMat));
}

void Scene::newSHADERS_updateUBOData() {
    if(grids.size() == 0)
        return;

    this->shouldUpdateUBOData = false;
    for (const auto& grid : this->grids) {
        this->setUniformBufferData(grid->uboHandle_colorAttributes, 0, 32, &grid->mainColorChannelAttributes());
        this->setUniformBufferData(grid->uboHandle_colorAttributes, 32, 32, &grid->colorChannelAttributes[0]);
        this->setUniformBufferData(grid->uboHandle_colorAttributes, 64, 32, &grid->colorChannelAttributes[1]);
        this->setUniformBufferData(grid->uboHandle_colorAttributes, 96, 32, &grid->colorChannelAttributes[2]);

    }

    auto& grid = this->grids[this->gridToDraw];
    float maxValue = grid->maxValue;
    glDeleteTextures(1, &grid->valuesRangeToDisplay);

    TextureUpload texParams;

    texParams.minmag.x         = GL_NEAREST;
    texParams.minmag.y         = GL_NEAREST;
    texParams.lod.y	           = -1000.f;
    texParams.wrap.s	       = GL_CLAMP_TO_EDGE;
    texParams.wrap.t	       = GL_CLAMP_TO_EDGE;

    texParams.internalFormat   = GL_RGB32F;
    texParams.size.y		   = 1;
    texParams.size.z		   = 1;
    texParams.format		   = GL_RGB;
    texParams.type		       = GL_FLOAT;

    std::vector<glm::vec3> data;
    std::vector<glm::vec3> data_color;
    for(int i = 0; i <= maxValue; ++i) {
        data.push_back(glm::vec3(0., 0., 0.));
        data_color.push_back(glm::vec3(0., 0., 0.));
    }
    for(int i = 0; i < grid->visu.size(); ++i) {
        if(grid->visu_visi[i]) {
            for(int j = grid->visu[i].first; j <= grid->visu[i].second; ++j) {
                if(j < data.size()) {
                    data[j] = glm::vec3(1., 1., 1.);
                    data_color[j] = grid->visu_color[i];
                }
            }
        }
    }

    texParams.size.x		   = data.size();
    texParams.data			   = data.data();

    grid->valuesRangeToDisplay = this->uploadTexture1D(texParams);

    texParams.size.x		   = data_color.size();
    texParams.data			   = data_color.data();

    grid->valuesRangeColorToDisplay = this->uploadTexture1D(texParams);

    grid->visu_map = data;
    grid->color_map = data_color;

    std::cout << "Update FBO" << std::endl;
    Q_EMIT meshMoved();// To update the 2D viewer
}

GLuint Scene::updateFBOOutputs(glm::ivec2 dimensions, GLuint fb_handle, GLuint old_texture) {
    // Silently ignore attempts to modify the 0-th FBO, which is provided by the
    // windowing system, and thus unmodifiable from GL/application code.
    if (fb_handle == 0) {
        return 0;
    }
    // Remove old textures, if updating the FBO from a resize event for example.
    if (old_texture != 0) {
        glDeleteTextures(1, &old_texture);
    }

    // Upload an _empty_ texture with no mipmapping, and nearest neighbor :
    GLuint new_tex = 0;
    TextureUpload t{};
    t.minmag		 = glm::ivec2{GL_NEAREST, GL_NEAREST};
    t.lod			 = glm::vec2{-1000.f, -1000.f};
    t.internalFormat = GL_RGBA32F;	  // RGBA 32-bit floating point, unnormalized
    t.size			 = glm::ivec3(dimensions, 0);

    // Create the texture object itself :
    new_tex = this->uploadTexture2D(t);
    if (new_tex == 0) {
        std::cerr << "Error : Could not create a framebuffer texture.\n";
        return 0;
    }

    // Bind framebuffer, and bind texture to a color attachment :
    glBindFramebuffer(GL_FRAMEBUFFER, fb_handle);
    // Bind texture 'new_tex' at level 0 to the color attachment 1 :
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, new_tex, 0);

    // Specify the texture ouptuts in the framebuffer state :
    GLenum colorAttachments[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, colorAttachments);

    // Check the framebuffer is complete :
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "An error occured while specifying the framebuffer attachments.\n";
    }

    return new_tex;
}

glm::vec4 Scene::readFramebufferContents(GLuint fb_handle, glm::ivec2 image_coordinates) {
    // The container of pixel values :
    glm::vec4 pixelValue = glm::vec4();

    // Bind the framebuffer to read from :
    glBindFramebuffer(GL_FRAMEBUFFER, fb_handle);
    // Specify reading from color attachment 1:
    glReadBuffer(GL_COLOR_ATTACHMENT1);
    // Read pixels :
    glReadPixels(image_coordinates.x, image_coordinates.y, 1, 1, GL_RGBA, GL_FLOAT, glm::value_ptr(pixelValue));

    return pixelValue;
}

void Scene::setupVAOPointers() {
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, this->vbo_VertPos);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, (void*) 0);

    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, this->vbo_VertNorm);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, (void*) 0);

    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, this->vbo_VertTex);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (void*) 0);
}

glm::vec3 Scene::getSceneBoundaries() const {
    return this->sceneBB.getDiagonal();
}

void Scene::setupVAOBoundingBox() {
    glBindVertexArray(this->vao_boundingBox);

    // Bind vertex buffer :
    glBindBuffer(GL_ARRAY_BUFFER, this->vbo_boundingBoxVertices);
    // Enable VAO pointer 0 : vertices
    glEnableVertexAttribArray(0);
    // Enable VAO pointer for 3-component vectors, non-normalized starting at index 0 :
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*) 0);
}

void Scene::drawBoundingBox(const Image::bbox_t& _box, glm::vec3 color, GLfloat* vMat, GLfloat* pMat) {
    glUseProgram(this->program_BoundingBox);

    glLineWidth(2.0f);

    // Locate uniforms :
    GLint location_pMat	   = glGetUniformLocation(this->program_BoundingBox, "pMat");
    GLint location_vMat	   = glGetUniformLocation(this->program_BoundingBox, "vMat");
    GLint location_bbColor = glGetUniformLocation(this->program_BoundingBox, "bbColor");
    GLint location_bbSize  = glGetUniformLocation(this->program_BoundingBox, "bbSize");
    GLint location_bbPos   = glGetUniformLocation(this->program_BoundingBox, "bbPos");

    Image::bbox_t::vec min	= _box.getMin();
    Image::bbox_t::vec diag = _box.getDiagonal();

    // Set uniforms :
    glUniformMatrix4fv(location_pMat, 1, GL_FALSE, pMat);
    glUniformMatrix4fv(location_vMat, 1, GL_FALSE, vMat);
    glUniform3fv(location_bbColor, 1, glm::value_ptr(color));
    glUniform3fv(location_bbSize, 1, glm::value_ptr(diag));
    glUniform3fv(location_bbPos, 1, glm::value_ptr(min));

    glBindVertexArray(this->vao_boundingBox);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->vbo_boundingBoxIndices);
    this->setupVAOBoundingBox();

    glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, (void*) 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glLineWidth(1.0f);
    glUseProgram(0);
}

uint Scene::colorFunctionToUniform(ColorFunction _c) {
    switch (_c) {
        case ColorFunction::SingleChannel:
            return 1;
        case ColorFunction::HistologyHandE:
            return 2;
        case ColorFunction::HSV2RGB:
            return 3;
        case ColorFunction::ColorMagnitude:
            return 4;
    }
    return 0;
}

void Scene::setColorFunction_r(ColorFunction _c) {
    if (_c == ColorFunction::SingleChannel && this->channels_r != ColorFunction::SingleChannel) {
        this->selectedChannel_r = 0;
    }
    if (_c == ColorFunction::SingleChannel && this->channels_r == ColorFunction::SingleChannel) {
        this->selectedChannel_r = (this->selectedChannel_r + 1) % 2;
    }
    this->channels_r = _c;
    // WARNING : below is a dirty hack, since UBO expects a color scale index but this gives it a constant
    // value that only has a meaning on the host (CPU) side.
    if (_c == ColorFunction::ColorMagnitude) {
        _c = ColorFunction::HSV2RGB;
    } else if (_c == ColorFunction::HSV2RGB) {
        _c = ColorFunction::HistologyHandE;
    }
    if(grids.size() > 0) {
        grids[0]->colorChannelAttributes[0].setColorScale(static_cast<int>(_c));
        grids[0]->colorChannelAttributes[1].setColorScale(static_cast<int>(_c));
    }
    this->shouldUpdateUBOData = true;
}

void Scene::setColorFunction_g(ColorFunction _c) {
    if (_c == ColorFunction::SingleChannel && this->channels_g != ColorFunction::SingleChannel) {
        this->selectedChannel_g = 0;
    }
    if (_c == ColorFunction::SingleChannel && this->channels_g == ColorFunction::SingleChannel) {
        this->selectedChannel_g = (this->selectedChannel_g + 1) % 2;
    }
    this->channels_g = _c;
    // WARNING : below is a dirty hack, since UBO expects a color scale index but this gives it a constant
    // value that only has a meaning on the host (CPU) side.
    if (_c == ColorFunction::HSV2RGB) {
        _c = ColorFunction::HistologyHandE;
    }
    if(grids.size() > 1) {
        grids[1]->colorChannelAttributes[0].setColorScale(static_cast<int>(_c));
        grids[1]->colorChannelAttributes[1].setColorScale(static_cast<int>(_c));
    }
    this->shouldUpdateUBOData = true;
}

void Scene::slotSetPlaneDisplacementX(float scalar) {
    this->planeDisplacement.x = scalar;
    Q_EMIT planesMoved(this->computePlanePositions());
}
void Scene::slotSetPlaneDisplacementY(float scalar) {
    this->planeDisplacement.y = scalar;
    Q_EMIT planesMoved(this->computePlanePositions());
}
void Scene::slotSetPlaneDisplacementZ(float scalar) {
    this->planeDisplacement.z = scalar;
    Q_EMIT planesMoved(this->computePlanePositions());
}

void Scene::slotTogglePlaneDirectionX() {
    this->planeDirection.x = -this->planeDirection.x;
}
void Scene::slotTogglePlaneDirectionY() {
    this->planeDirection.y = -this->planeDirection.y;
}
void Scene::slotTogglePlaneDirectionZ() {
    this->planeDirection.z = -this->planeDirection.z;
}
void Scene::toggleAllPlaneDirections() {
    this->planeDirection = -this->planeDirection;
}

void Scene::slotTogglePlaneX(bool display) {
    if(this->planeActivation[0] != 0.) {
        this->planeActivation[0] = 0.;
    } else {
        this->planeActivation[0] = 1.;
    }
}

void Scene::slotTogglePlaneY(bool display) {
    if(this->planeActivation[1] != 0.) {
        this->planeActivation[1] = 0.;
    } else {
        this->planeActivation[1] = 1.;
    }
}

void Scene::slotTogglePlaneZ(bool display) {
    if(this->planeActivation[2] != 0.) {
        this->planeActivation[2] = 0.;
    } else {
        this->planeActivation[2] = 1.;
    }
}

void Scene::slotSetMinTexValue(double val) {
    this->textureBounds0.x = val;
    this->updateCVR();
}
void Scene::slotSetMaxTexValue(double val) {
    this->textureBounds0.y = val;
    this->updateCVR();
}
void Scene::slotSetMinTexValueAlternate(double val) {
    this->textureBounds1.x = val;
    this->updateCVR();
}
void Scene::slotSetMaxTexValueAlternate(double val) {
    this->textureBounds1.y = val;
    this->updateCVR();
}

void Scene::slotSetMinColorValue(double val) {
    this->colorBounds0.x = val;
    this->updateCVR();
}
void Scene::slotSetMaxColorValue(double val) {
    this->colorBounds0.y = val;
    this->updateCVR();
}
void Scene::slotSetMinColorValueAlternate(double val) {
    this->colorBounds1.x = val;
    this->updateCVR();
}
void Scene::slotSetMaxColorValueAlternate(double val) {
    this->colorBounds1.y = val;
    this->updateCVR();
}

Image::bbox_t Scene::getSceneBoundingBox() const {
    return this->sceneBB;
}

void Scene::setColor0(qreal r, qreal g, qreal b) {
    glm::vec<3, qreal, glm::highp> qtcolor(r, g, b);
    this->color0 = glm::convert_to<float>(qtcolor);
    if(this->grids.size() > 0)
        this->grids[0]->color_0 = this->color0;
    this->signal_updateUserColorScales();
    emit this->colorChanged();
    return;
}

void Scene::setColor1(qreal r, qreal g, qreal b) {
    glm::vec<3, qreal, glm::highp> qtcolor(r, g, b);
    this->color1 = glm::convert_to<float>(qtcolor);
    if(this->grids.size() > 0)
        this->grids[0]->color_1 = this->color1;
    this->signal_updateUserColorScales();
    emit this->colorChanged();
    return;
}

void Scene::setColor0Alternate(qreal r, qreal g, qreal b) {
    glm::vec<3, qreal, glm::highp> qtcolor(r, g, b);
    this->color0_second = glm::convert_to<float>(qtcolor);
    if(this->grids.size() > 1)
        this->grids[1]->color_0 = this->color0;
    this->signal_updateUserColorScales();
    emit this->colorChanged();
    return;
}

void Scene::setColor1Alternate(qreal r, qreal g, qreal b) {
    glm::vec<3, qreal, glm::highp> qtcolor(r, g, b);
    this->color1_second = glm::convert_to<float>(qtcolor);
    if(this->grids.size() > 1)
        this->grids[1]->color_1 = this->color1;
    this->signal_updateUserColorScales();
    emit this->colorChanged();
    return;
}

void Scene::signal_updateUserColorScales() {
    this->shouldUpdateUserColorScales = true;
}

void Scene::newSHADERS_updateUserColorScales() {
    this->shouldUpdateUserColorScales = false;
    TextureUpload colorScaleUploadParameters;
    std::size_t textureSize = this->maximumTextureSize / 2u;
    float textureSize_f		= static_cast<float>(this->maximumTextureSize / 2u);
    std::vector<glm::vec3> colorScaleData_user0(textureSize);
    std::vector<glm::vec3> colorScaleData_user1(textureSize);

    // The color scale 0 first :
    for (std::size_t i = 0; i < textureSize; ++i) {
        colorScaleData_user0[i] = glm::mix(this->color0, this->color1, static_cast<float>(i) / textureSize_f);
        colorScaleData_user1[i] = glm::mix(this->color0_second, this->color1_second, static_cast<float>(i) / textureSize_f);
    }

    colorScaleUploadParameters.minmag.x	 = GL_LINEAR;
    colorScaleUploadParameters.minmag.y	 = GL_LINEAR;
    colorScaleUploadParameters.lod.y	 = -1000.f;
    colorScaleUploadParameters.wrap.x	 = GL_CLAMP_TO_EDGE;
    colorScaleUploadParameters.wrap.y	 = GL_CLAMP_TO_EDGE;
    colorScaleUploadParameters.wrap.z	 = GL_CLAMP_TO_EDGE;
    colorScaleUploadParameters.swizzle.r = GL_RED;
    colorScaleUploadParameters.swizzle.g = GL_GREEN;
    colorScaleUploadParameters.swizzle.b = GL_BLUE;
    colorScaleUploadParameters.swizzle.a = GL_ONE;

    colorScaleUploadParameters.level		  = 0;
    colorScaleUploadParameters.internalFormat = GL_RGB;
    colorScaleUploadParameters.size.x		  = textureSize;
    colorScaleUploadParameters.size.y		  = 1;
    colorScaleUploadParameters.size.z		  = 1;
    colorScaleUploadParameters.format		  = GL_RGB;
    colorScaleUploadParameters.type			  = GL_FLOAT;
    colorScaleUploadParameters.data			  = colorScaleData_user0.data();
    this->tex_colorScale_user0		  = this->uploadTexture1D(colorScaleUploadParameters);

    colorScaleUploadParameters.data	 = colorScaleData_user1.data();
    this->tex_colorScale_user1 = this->uploadTexture1D(colorScaleUploadParameters);
}

void Scene::updateCVR() {
    // For all grids, signal they need to be
    int i = 0;
    for (const auto& grid : this->grids) {

        glm::vec2 textureBounds = this->textureBounds0;
        glm::vec2 colorBounds = this->colorBounds0;
        if(i > 0) {
            textureBounds = this->textureBounds1;
            colorBounds = this->colorBounds1;
        }

        grid->colorChannelAttributes[0].setMinVisible(textureBounds.x);
        grid->colorChannelAttributes[0].setMaxVisible(textureBounds.y);
        grid->colorChannelAttributes[0].setMinColorScale(colorBounds.x);
        grid->colorChannelAttributes[0].setMaxColorScale(colorBounds.y);
        grid->colorChannelAttributes[1].setMinVisible(textureBounds.x);
        grid->colorChannelAttributes[1].setMaxVisible(textureBounds.y);
        grid->colorChannelAttributes[1].setMinColorScale(colorBounds.x);
        grid->colorChannelAttributes[1].setMaxColorScale(colorBounds.y);
        i++;
        // For now handle only one canal
        //grid->colorChannelAttributes[1].setMinVisible(this->textureBounds1.x);
        //grid->colorChannelAttributes[1].setMaxVisible(this->textureBounds1.y);
        //grid->colorChannelAttributes[1].setMinColorScale(this->colorBounds1.x);
        //grid->colorChannelAttributes[1].setMaxColorScale(this->colorBounds1.y);
    }

    this->shouldUpdateUBOData = true;
}

bool compPt(std::pair<glm::vec4, std::vector<std::vector<int>>> i, std::pair<glm::vec4, std::vector<std::vector<int>>> j) {
    for (int x = 2; x >= 0; --x) {
        if (i.first[x] < j.first[x])
            return true;

        if (i.first[x] > j.first[x])
            return false;
    }

    return false;
}

bool contain(const InfoToSend& value, const InfoToSend& contain) {
    return value & contain > 0;
}

// TODO: replace this function by a real update function
void Scene::sendFirstTetmeshToGPU() {
    std::cout << "Send tetmesh " << this->gridToDraw << std::endl;
    if(this->grids.size() > 0)
        this->sendTetmeshToGPU(this->gridToDraw, InfoToSend(InfoToSend::VERTICES | InfoToSend::NORMALS), this->sortingRendering);
    //if(this->meshManipulator) {
    //    this->meshManipulator->setAllManipulatorsPosition(this->getBaseMesh(this->activeMesh)->getMeshPositions());
    //}
    Q_EMIT meshMoved();
}

void Scene::sendTetmeshToGPU(int gridIdx, const InfoToSend infoToSend, bool sort) {
    std::cout << "Send to GPU" << std::endl;
    if(sort)
        std::cout << "Sorting" << std::endl;

    std::size_t vertWidth = 0, vertHeight = 0;
    std::size_t normWidth = 0, normHeight = 0;
    std::size_t coorWidth = 0, coorHeight = 0;
    std::size_t neighbWidth = 0, neighbHeight = 0;

    //TetMesh& newMesh = *this->grids[gridIdx]->grid->grid->tetmesh;
    TetMesh& newMesh = *this->grids[gridIdx]->grid;

    if(contain(infoToSend, InfoToSend::NORMALS)) {
        newMesh.computeNormals();
    }
    __GetTexSize(newMesh.mesh.size() * 4 * 3, &vertWidth, &vertHeight);
    __GetTexSize(newMesh.mesh.size() * 4, &normWidth, &normHeight);
    __GetTexSize(newMesh.mesh.size() * 4 * 3, &coorWidth, &coorHeight);
    __GetTexSize(newMesh.mesh.size() * 4, &neighbWidth, &neighbHeight);

    this->grids[gridIdx]->volumetricMesh.tetrahedraCount = newMesh.mesh.size();

    GLfloat* rawVertices  = new GLfloat[vertWidth * vertHeight * 3];
    GLfloat* rawNormals	  = new GLfloat[normWidth * normHeight * 4];
    GLfloat* tex		  = new GLfloat[coorWidth * coorHeight * 3];
    GLfloat* rawNeighbors = new GLfloat[neighbWidth * neighbHeight * 3];

    int iNeigh = 0;
    int iPt = 0;
    int iNormal = 0;
    std::vector<std::pair<int, float>> orderedTets;
    if(sort) {
        newMesh.sortTet(this->cameraPosition, orderedTets);
    }

    for (int idx = 0; idx < newMesh.mesh.size(); idx++) {
        int tetIdx = idx;
        if(sort)
            tetIdx = orderedTets[idx].first;
        const Tetrahedron& tet = newMesh.mesh[tetIdx];
        for(int faceIdx = 0; faceIdx < 4; ++faceIdx) {

            if(contain(infoToSend, InfoToSend::NEIGHBORS)) {
                if(sort) {
                    std::vector<std::pair<int, float>>::iterator i = std::find_if(
                        orderedTets.begin(), orderedTets.end(),
                        [&](const std::pair<int, float>& x) { return x.first == tet.neighbors[faceIdx];});
                    rawNeighbors[iNeigh] = static_cast<GLfloat>(i - orderedTets.begin());
                } else {
                    rawNeighbors[iNeigh] = static_cast<GLfloat>(tet.neighbors[faceIdx]);
                }
                iNeigh += 3;
            }

            if(contain(infoToSend, InfoToSend::NORMALS)) {
                for (int i = 0; i < 4; ++i) {
                    rawNormals[iNormal++] = tet.normals[faceIdx][i];
                    //rawNormals[iNormal++] = glm::normalize((newMesh.getModelMatrix() * tet.normals[faceIdx]))[i];
                    //rawNormals[iNormal++] = glm::vec4(1., 0., 0., 1.)[i];
                }
            }

            for (int k = 0; k < 3; ++k) {
                int ptIndex = tet.getPointIndex(faceIdx, k);
                for (int i = 0; i < 3; ++i) {
                    if(contain(infoToSend, InfoToSend::VERTICES))
                        rawVertices[iPt] = newMesh.getVertice(ptIndex)[i];
                    if(contain(infoToSend, InfoToSend::TEXCOORD))
                        tex[iPt] = newMesh.texCoord[ptIndex][i];
                    iPt++;
                }
            }
        }
    }

    // Struct to upload the texture to OpenGL :
    TextureUpload texParams = {};
    texParams.minmag.x = GL_NEAREST;
    texParams.minmag.y = GL_NEAREST;
    texParams.lod.y	   = -1000.f;
    texParams.wrap.s   = GL_CLAMP;
    texParams.wrap.t   = GL_CLAMP;

    if(contain(infoToSend, InfoToSend::VERTICES)) {
        texParams.internalFormat				   = GL_RGB32F;
        texParams.size.x						   = vertWidth;
        texParams.size.y						   = vertHeight;
        texParams.format						   = GL_RGB;
        texParams.type							   = GL_FLOAT;
        texParams.data							   = rawVertices;
        glDeleteTextures(1, &this->grids[gridIdx]->volumetricMesh.vertexPositions);
        this->grids[gridIdx]->volumetricMesh.vertexPositions = this->uploadTexture2D(texParams);
    }

    if(contain(infoToSend, InfoToSend::NORMALS)) {
        texParams.internalFormat			   = GL_RGBA32F;
        texParams.size.x					   = normWidth;
        texParams.size.y					   = normHeight;
        texParams.format					   = GL_RGBA;
        texParams.data						   = rawNormals;
        glDeleteTextures(1, &this->grids[gridIdx]->volumetricMesh.faceNormals);
        this->grids[gridIdx]->volumetricMesh.faceNormals = this->uploadTexture2D(texParams);
    }

    if(contain(infoToSend, InfoToSend::TEXCOORD)) {
        texParams.internalFormat					  = GL_RGB32F;
        texParams.size.x							  = coorWidth;
        texParams.size.y							  = coorHeight;
        texParams.format							  = GL_RGB;
        texParams.data								  = tex;
        glDeleteTextures(1, &this->grids[gridIdx]->volumetricMesh.textureCoordinates);
        this->grids[gridIdx]->volumetricMesh.textureCoordinates = this->uploadTexture2D(texParams);
    }

    if(contain(infoToSend, InfoToSend::NEIGHBORS)) {
        texParams.size.x						= neighbWidth;
        texParams.size.y						= neighbHeight;
        texParams.data							= rawNeighbors;
        glDeleteTextures(1, &this->grids[gridIdx]->volumetricMesh.neighborhood);
        this->grids[gridIdx]->volumetricMesh.neighborhood = this->uploadTexture2D(texParams);
    }

    delete[] tex;
    delete[] rawVertices;
    delete[] rawNormals;
    delete[] rawNeighbors;
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Scene::tex3D_buildBuffers(VolMesh& volMesh) {
    // Tetra ///////////////////////////////////////////////////////////////////////
    //    v0----- v
    //   /       /|
    //  v ------v3|
    //  | |     | |
    //  | |v ---|-|v2
    //  |/      |/
    //  v1------v

    float v0[3] = {-1, -1, 1};
    float v1[3] = {-1, 1, -1};
    float v2[3] = {1, -1, -1};
    float v3[3] = {1, 1, 1};

    // vertex coords array
    GLfloat vertices[] = {v3[0], v3[1], v3[2], v1[0], v1[1], v1[2], v2[0], v2[1], v2[2],	// v3-v1-v2
      v3[0], v3[1], v3[2], v2[0], v2[1], v2[2], v1[0], v1[1], v1[2],	// v3-v2-v1
      v3[0], v3[1], v3[2], v0[0], v0[1], v0[2], v1[0], v1[1], v1[2],	// v3-v0-v1
      v2[0], v2[1], v2[2], v1[0], v1[1], v1[2], v0[0], v0[1], v0[2]};	 // v2-v1-v0
    // normal array
    GLfloat normals[] = {v3[0], v3[1], v3[2], v1[0], v1[1], v1[2], v2[0], v2[1], v2[2],	   // v3-v1-v2
      v3[0], v3[1], v3[2], v2[0], v2[1], v2[2], v1[0], v1[1], v1[2],	// v3-v2-v1
      v3[0], v3[1], v3[2], v0[0], v0[1], v0[2], v1[0], v1[1], v1[2],	// v3-v0-v1
      v2[0], v2[1], v2[2], v1[0], v1[1], v1[2], v0[0], v0[1], v0[2]};	 // v2-v1-v0
    // index array of vertex array for glDrawElements()
    // Notice the indices are listed straight from beginning to end as exactly
    // same order of vertex array without hopping, because of different normals at
    // a shared vertex. For this case, glDrawArrays() and glDrawElements() have no
    // difference.
    GLushort indices[] = {0, 1, 2,
      3, 4, 5,
      6, 7, 8,
      9, 10, 11};
    // texture coords :
    GLfloat textureCoords[] = {0., 0., 1., 1., 2., 2.,
      3., 3., 4., 4., 5., 5.,
      6., 6., 7., 7., 8., 8.,
      9., 9., 10., 10., 11., 11.};

    glBindBuffer(GL_ARRAY_BUFFER, this->vbo_Texture3D_VertPos);
    glBufferData(GL_ARRAY_BUFFER, 12 * 3 * sizeof(GLfloat), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, this->vbo_Texture3D_VertNorm);
    glBufferData(GL_ARRAY_BUFFER, 12 * 3 * sizeof(GLfloat), normals, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, this->vbo_Texture3D_VertTex);
    glBufferData(GL_ARRAY_BUFFER, 12 * 2 * sizeof(GLfloat), textureCoords, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->vbo_Texture3D_VertIdx);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 12 * sizeof(GLushort), indices, GL_STATIC_DRAW);

    glBindVertexArray(this->vao_VolumetricBuffers);
    this->tex3D_bindVAO();
}

void Scene::tex3D_bindVAO() {
    glBindVertexArray(this->vao_VolumetricBuffers);

    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, this->vbo_Texture3D_VertPos);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*) 0);

    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, this->vbo_Texture3D_VertNorm);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, (void*) 0);

    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, this->vbo_Texture3D_VertTex);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, (void*) 0);
}

/**********************************************************************/
/**********************************************************************/

SceneGL::SceneGL() {
    this->context		= nullptr;
}

SceneGL::~SceneGL(void) {
}

void SceneGL::initGl(QOpenGLContext* _context) {
    // Check if the scene has been initialized, share contexts if it has been :
    this->context = _context;
    if (this->initializeOpenGLFunctions() == false) {
        throw std::runtime_error("Could not initialize OpenGL functions for Scene GL.");
    }
}

GLuint SceneGL::uploadTexture1D(const TextureUpload& tex) {
    if (this->context != nullptr) {
        if (this->context->isValid() == false) {
            throw std::runtime_error("No associated valid context");
        }
    } else {
        throw std::runtime_error("nullptr as context");
    }

    glEnable(GL_TEXTURE_1D);

    GLuint texHandle = 0;
    glGenTextures(1, &texHandle);
    glBindTexture(GL_TEXTURE_1D, texHandle);

    // Min and mag filters :
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, tex.minmag.x);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, tex.minmag.y);

    // Set the min and max LOD values :
    glTexParameterf(GL_TEXTURE_1D, GL_TEXTURE_MIN_LOD, tex.lod.x);
    glTexParameterf(GL_TEXTURE_1D, GL_TEXTURE_MAX_LOD, tex.lod.y);

    // Set the wrap parameters :
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, tex.wrap.x);

    // Set the swizzle the user wants :
    glTexParameteriv(GL_TEXTURE_1D, GL_TEXTURE_SWIZZLE_RGBA, glm::value_ptr(tex.swizzle));

    // Set the pixel alignment :
    glPixelStorei(GL_PACK_ALIGNMENT, tex.alignment.x);
    glPixelStorei(GL_UNPACK_ALIGNMENT, tex.alignment.y);

    glTexImage1D(GL_TEXTURE_1D,	   // GLenum : Target
      static_cast<GLint>(tex.level),	// GLint  : Level of detail of the current texture (0 = original)
      tex.internalFormat,	 // GLint  : Number of color components in the picture.
      tex.size.x,	 // GLsizei: Image width
      static_cast<GLint>(0),	// GLint  : Border. This value MUST be 0.
      tex.format,	 // GLenum : Format of the pixel data
      tex.type,	   // GLenum : Type (the data type as in uchar, uint, float ...)
      tex.data	  // void*  : Data to load into the buffer
    );

    return texHandle;
}

GLuint SceneGL::uploadTexture2D(const TextureUpload& tex) {
    if (this->context != nullptr) {
        if (this->context->isValid() == false) {
            throw std::runtime_error("No associated valid context");
        }
    } else {
        throw std::runtime_error("nullptr as context");
    }

    glEnable(GL_TEXTURE_2D);

    GLuint texHandle = 0;
    glGenTextures(1, &texHandle);
    glBindTexture(GL_TEXTURE_2D, texHandle);

    // Min and mag filters :
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, tex.minmag.x);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, tex.minmag.y);

    // Set the min and max LOD values :
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, tex.lod.x);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, tex.lod.y);

    // Set the wrap parameters :
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, tex.wrap.x);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, tex.wrap.y);

    // Set the swizzle the user wants :
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, glm::value_ptr(tex.swizzle));

    // Set the pixel alignment :
    glPixelStorei(GL_PACK_ALIGNMENT, tex.alignment.x);
    glPixelStorei(GL_UNPACK_ALIGNMENT, tex.alignment.y);

    glTexImage2D(GL_TEXTURE_2D,	   // GLenum : Target
      static_cast<GLint>(tex.level),	// GLint  : Level of detail of the current texture (0 = original)
      tex.internalFormat,	 // GLint  : Number of color components in the picture. Here grayscale so GL_RED
      tex.size.x,	 // GLsizei: Image width
      tex.size.y,	 // GLsizei: Image height
      static_cast<GLint>(0),	// GLint  : Border. This value MUST be 0.
      tex.format,	 // GLenum : Format of the pixel data
      tex.type,	   // GLenum : Type (the data type as in uchar, uint, float ...)
      tex.data	  // void*  : Data to load into the buffer
    );

    return texHandle;
}

/*********************************/
/* Slots */
/*********************************/

void Scene::toggleDisplayTetmesh(bool value) {
    this->displayTetmesh = value;
}

void Scene::setColorChannel(ColorChannel mode) {
    this->rgbMode = mode;
    std::for_each(this->grids.begin(), this->grids.end(), [this](GridGLView::Ptr& gridView) {
        switch (this->rgbMode) {
            case ColorChannel::None:
                gridView->colorChannelAttributes[0].setHidden();
                gridView->colorChannelAttributes[1].setHidden();
                break;
            case ColorChannel::RedOnly:
                gridView->colorChannelAttributes[0].setHidden(false);
                gridView->colorChannelAttributes[1].setHidden();
                break;
            case ColorChannel::GreenOnly:
                gridView->colorChannelAttributes[0].setHidden();
                gridView->colorChannelAttributes[1].setHidden(false);
                break;
            case ColorChannel::RedAndGreen:
                gridView->colorChannelAttributes[0].setHidden(false);
                gridView->colorChannelAttributes[1].setHidden(false);
                break;
            default:
                // do nothing
                break;
        }
    });
    this->shouldUpdateUBOData = true;
    switch (mode) {
        case ColorChannel::None:
            std::cerr << "Set mode to none" << '\n';
            break;
        case ColorChannel::RedOnly:
            std::cerr << "Set mode to red" << '\n';
            break;
        case ColorChannel::GreenOnly:
            std::cerr << "Set mode to green" << '\n';
            break;
        case ColorChannel::RedAndGreen:
            std::cerr << "Set mode to both channels" << '\n';
            break;
        case ColorChannel::HandEColouring:
            std::cerr << "Set mode to both channels" << '\n';
            break;
        default:
            std::cerr << "Cannot set unknown mode\n";
            break;
    }
}

bool Scene::openGraph(const std::string& name, const std::string& filename, const glm::vec4& color) {
    //this->surfaceMesh = new SurfaceMesh("/home/thomas/data/Data/Mesh/bunny_lowres.off");
    this->graph_meshes.push_back(std::pair<GraphMesh*, std::string>(nullptr, name));
    this->graph_meshes.back().first = new GraphMesh(filename);

    Q_EMIT meshAdded(name, false, false);
    this->changeActiveMesh(name);
    return true;
}

bool Scene::openMesh(const std::string& name, const std::string& filename, const glm::vec4& color) {
    //this->surfaceMesh = new SurfaceMesh("/home/thomas/data/Data/Mesh/bunny_lowres.off");
    this->meshes.push_back(std::pair<SurfaceMesh*, std::string>(nullptr, name));
    this->meshes.back().first = new SurfaceMesh(filename);
    this->meshes.back().first->initializeGL(this);
    this->meshes.back().first->color = color;

    Q_EMIT meshAdded(name, false, false);
    this->changeActiveMesh(name);
    return true;
}

bool Scene::linkCage(const std::string& cageName, BaseMesh * meshToDeform, const bool MVC) {
    SurfaceMesh * cage = this->getMesh(cageName);

    if(!cage) {
        std::cout << "ERROR: no cage mesh provided" << std::endl;
        return false;
    }

    if(!meshToDeform) {
        std::cout << "ERROR: no surface mesh provided" << std::endl;
        return false;
    }

    int cageIdx = this->getMeshIdx(cageName);
    //delete this->meshes[cageIdx].first;
    glm::vec4 color = this->meshes[cageIdx].first->color;
    delete this->meshes[cageIdx].first;

    TetMesh * tetMesh = dynamic_cast<TetMesh*>(meshToDeform);
    if(tetMesh) {
        //this->meshes[cageIdx].first = new CageGreenLRI(this->meshes[cageIdx].first, tetMesh);
    } else {
        if(MVC)
            this->meshes[cageIdx].first = new CageMVC(this->meshes[cageIdx].first, meshToDeform);
        else
            this->meshes[cageIdx].first = new CageGreen(this->meshes[cageIdx].first, meshToDeform);
    }

    this->meshes[cageIdx].first->initializeGL(this);
    this->meshes[cageIdx].first->color = color;

    //this->changeActiveMesh(name);

    //this->updateSceneBBox(this->meshes.back().first->bbMin, this->meshes.back().first->bbMax);
    //this->updateSceneCenter();

    // TODO change this
    //Q_EMIT meshAdded(name, false, true);
    return true;
}

bool Scene::openCage(const std::string& name, const std::string& filename, BaseMesh * surfaceMeshToDeform, const bool MVC, const glm::vec4& color) {
    //this->cage = new CageMVC("/home/thomas/data/Data/Mesh/bunny_cage.off", this->surfaceMesh);
    if(!surfaceMeshToDeform) {
        std::cout << "ERROR: no surface mesh provided" << std::endl;
        return false;
    }
    this->meshes.push_back(std::pair<SurfaceMesh*, std::string>(nullptr, name));

    TetMesh * tetMesh = dynamic_cast<TetMesh*>(surfaceMeshToDeform);
    if(MVC) {
        this->meshes.back().first = new CageMVC(filename, surfaceMeshToDeform);
    } else {
        if(tetMesh)
            this->meshes.back().first = new CageGreenLRI(filename, tetMesh);
        else
            this->meshes.back().first = new CageGreen(filename, surfaceMeshToDeform);
    }

    this->meshes.back().first->initializeGL(this);
    this->meshes.back().first->color = color;

    Q_EMIT meshAdded(name, false, true);
    this->changeActiveMesh(name);
    return true;
}

bool Scene::openGrid(const std::string& name, const std::vector<std::string>& imgFilenames, const int subsample, const glm::vec3& sizeVoxel, const glm::vec3& nbCubeGridTransferMesh) {
    int autofitSubsample = this->autofitSubsample(subsample, imgFilenames);
    Grid * newGrid = new Grid(imgFilenames, autofitSubsample, sizeVoxel, nbCubeGridTransferMesh);
    this->addGridToScene(name, newGrid);
    return true;
}

bool Scene::openGrid(const std::string& name, const std::vector<std::string>& imgFilenames, const int subsample, const std::string& transferMeshFileName) {
    int autofitSubsample = this->autofitSubsample(subsample, imgFilenames);
    //TODO: sizeVoxel isn't take into account with loading a custom transferMesh
    Grid * newGrid = new Grid(imgFilenames, autofitSubsample, glm::vec3(0., 0., 0.), transferMeshFileName);
    this->addGridToScene(name, newGrid);
    return true;
}

void Scene::addGridToScene(const std::string& name, Grid * newGrid) {
    GridGLView::Ptr gridView = std::make_shared<GridGLView>(newGrid);
    this->grids.push_back(gridView);

    this->gridToDraw += 1;

    this->addGrid();
    this->grids_name.push_back(name);

    this->updateSceneCenter();
    std::cout << "New grid added with BBox:" << this->grids.back()->grid->bbMax << std::endl;

    Q_EMIT meshAdded(name, true, false);
    this->changeActiveMesh(name);

    this->gridsToDraw.push_back(this->gridToDraw);
}

int Scene::autofitSubsample(int initialSubsample, const std::vector<std::string>& imgFilenames) {
    float percentageOfMemory = 0.7;
    int gpuMemoryInGB = 2;
    double gpuMemoryInBytes = double(gpuMemoryInGB) * double(1073741824.);

    int finalSubsample = initialSubsample;
    bool autofitSizeRequired = false;
    bool autofitMemoryRequired = false;

    TIFFReader tiffReader(imgFilenames);
    glm::vec3 imgResolution = tiffReader.getImageResolution();
    float maxResolution = std::max(imgResolution[0], std::max(imgResolution[1], imgResolution[2]));
    if(maxResolution > this->maximumTextureSize) {
        autofitSizeRequired = true;
        std::cout << "INFO: image too large to fit in the GPU, size " << imgResolution << std::endl;
    }

    if(autofitSizeRequired) {// Auto-fit size activated
        std::cout << "Auto-fit size activated" << std::endl;
        finalSubsample = std::max(2.f, std::ceil(maxResolution / (float(this->maximumTextureSize))));
        std::cout << "Subsample set to [" << finalSubsample << "]" << std::endl;
    }

    double dataMemory = 1;
    for(int dim = 0; dim < 3; ++dim) {
        dataMemory *= imgResolution[dim];// Because data are cast as uint16_t internally
    }
    dataMemory *= 4;
    std::cout << "GPU memory available: [" << gpuMemoryInBytes / 1073741824. << "] Go" << std::endl;
    std::cout << "Data memory usage: [" << dataMemory / 1073741824. << "] Go" << std::endl;
    if(gpuMemoryInBytes < dataMemory) {
        autofitMemoryRequired = true;
        std::cout << "INFO: image too large to fit in the GPU, memory [" << dataMemory / 1073741824. << "] Go" << std::endl;
    }

    autofitMemoryRequired = false;
    if(autofitMemoryRequired) {
        std::cout << "Auto-fit memory activated" << std::endl;
        finalSubsample = std::max(2., dataMemory / (gpuMemoryInBytes*double(percentageOfMemory)));// We want to fill 70% of the memory
        std::cout << "Subsample set to [" << finalSubsample << "]" << std::endl;
        std::cout << "GPU memory used by the data: [" << percentageOfMemory * 100. << "%]" << std::endl;
        std::cout << "GPU memory usage reduced to: [" << (dataMemory/double(finalSubsample))/1073741824. << "] Go" << std::endl;
    }
    return finalSubsample;
}

int Scene::getMeshIdx(const std::string& name) {
    for(int i = 0; i < this->meshes.size(); ++i) {
        if(this->meshes[i].second == name) {
            return i;
        }
    }
    std::cout << "Error: wrong mesh name to get idx: [" << name << "]" << std::endl;
    return -1;
}

SurfaceMesh * Scene::getMesh(const std::string& name) {
    for(int i = 0; i < this->meshes.size(); ++i) {
        if(this->meshes[i].second == name) {
            return this->meshes[i].first;
        }
    }
    std::cout << "Error: wrong mesh name: [" << name << "]" << std::endl;
    return nullptr;
}

Cage * Scene::getCage(const std::string& name) {
    return dynamic_cast<Cage*>(this->getMesh(name));
}

void Scene::changeSceneRadius(float sceneRadius) {
    if(this->graph_meshes.size() > 0)
        this->graph_meshes.front().first->zoom(sceneRadius);
    if(this->meshManipulator) {
        this->meshManipulator->zoom(sceneRadius);
    }
}

void Scene::updateSceneRadius() {
    Q_EMIT sceneRadiusChanged(this->getSceneRadius());
    this->changeSceneRadius(this->distanceFromCamera);
}

float Scene::getSceneRadius() {
    BaseMesh * mesh = this->getBaseMesh(this->activeMesh);
    if(mesh) {
        return glm::length(mesh->getDimensions());
    }
    return 1.;
}


glm::vec3 Scene::getSceneCenter() {

    BaseMesh * mesh = this->getBaseMesh(this->activeMesh);
    if(mesh) {
        std::cout << "Update scene center to origin of [" << this->activeMesh << "] which is " << mesh->getOrigin() << std::endl;
        return mesh->getOrigin();
    } //else {
        //return (this->sceneBBMax + this->sceneBBMin)/2.f;
    //}
    return glm::vec3(0., 0., 0.);
}

void Scene::updateSceneCenter() {
    Q_EMIT sceneCenterChanged(this->getSceneCenter());
}

void Scene::toggleBindMeshToCageMove() {
    this->toggleBindMeshToCageMove(this->activeMesh);
}

void Scene::toggleBindMeshToCageMove(const std::string& name) {
    Cage * cage = this->getCage(name);
    if(cage) {
        if(cage->moveMeshToDeform) {
            cage->unbindMovementWithDeformedMesh();
        } else {
            cage->bindMovementWithDeformedMesh();
        }
    } else {
        std::cout << "Error: the selected mesh isn't a cage or doesn't exist" << std::endl;
    }
}

void Scene::setBindMeshToCageMove(const std::string& name, bool state) {
    Cage * cage = this->getCage(name);
    if(cage) {
        if(state) {
            cage->bindMovementWithDeformedMesh();
        } else {
            cage->unbindMovementWithDeformedMesh();
        }
    } else {
        std::cout << "Error: the selected mesh isn't a cage or doesn't exist" << std::endl;
    }
}

bool Scene::isGrid(const std::string& name) {
    for(int i = 0; i < this->grids_name.size(); ++i) {
        if(this->grids_name[i] == name) {
            return true;
        }
    }
    return false;
}

int Scene::getGridIdxLinkToCage(const std::string& name) {
    for(int i = 0; i < this->cageToGrid.size(); ++i) {
        if(this->cageToGrid[i].first == name) {
            return this->getGridIdx(this->cageToGrid[i].second);
        }
    }
    return -1;
}

int Scene::getGridIdx(const std::string& name) {
    for(int i = 0; i < this->grids_name.size(); ++i) {
        if(this->grids_name[i] == name) {
            return i;
        }
    }
    return -1;
}

std::pair<glm::vec3, glm::vec3> Scene::getBbox(const std::string& name) {
    BaseMesh * mesh = this->getBaseMesh(name);
    if(mesh) {
        return std::make_pair(mesh->bbMin, mesh->bbMax);
    } else {
        return std::make_pair(glm::vec3(0., 0., 0.), glm::vec3(0., 0., 0.));
    }
}

void Scene::openAtlas() {
         ///home/thomas/data/Data/teletravail/
         ///home/thomas/data/Data/Demo
         //this->openGrid(std::string("atlas"), {std::string("/home/thomas/data/Data/teletravail/atlas.tiff")}, 1, std::string("/home/thomas/data/Data/teletravail/atlas-transfert.mesh"));
         this->openGrid(std::string("atlas"), {std::string("/home/thomas/data/Data/Demo/atlas/atlas.tiff")}, 1, std::string("/home/thomas/data/Data/Demo/atlas/atlas-transfert.mesh"));
         //this->openCage(std::string("cage"), std::string("/home/thomas/data/Data/teletravail/atlas-cage-hyperdilated.off"), std::string("atlas"), true);
         this->openCage(std::string("cage"), std::string("/home/thomas/data/Data/Demo/atlas/atlas-cage_fixed.off"), std::string("atlas"), true);
         this->getCage(std::string("cage"))->setARAPDeformationMethod();
         this->getCage(std::string("cage"))->unbindMovementWithDeformedMesh();
         this->getCage(std::string("cage"))->setOrigin(this->getBaseMesh("atlas")->getOrigin());
         this->getCage(std::string("cage"))->bindMovementWithDeformedMesh();

         //this->applyCage(std::string("cage"), std::string("/home/thomas/data/Data/teletravail/atlas-cage-hyperdilated-rigidRegister-lightsheet_2.off"));

         //this->getCage(std::string("cage"))->unbindMovementWithDeformedMesh();
         //this->applyCage(std::string("cage"), std::string("/home/thomas/data/Data/Data/good_cage.off"));
         //this->getCage(std::string("cage"))->bindMovementWithDeformedMesh();

         this->changeActiveMesh("cage");
 }

 void Scene::openIRM() {
         this->openGrid(std::string("irm"), {std::string("/home/thomas/data/Data/teletravail/irm.tif")}, 1, glm::vec3(3.9, 3.9, 50));
         this->changeActiveMesh("irm");
 }

void Scene::init() {
}

BaseMesh * Scene::getBaseMesh(const std::string& name) {
    SurfaceMesh * surface = nullptr;
    for(int i = 0; i < this->meshes.size(); ++i) {
        if(this->meshes[i].second == name) {
            return this->meshes[i].first;
        }
    }
    for(int i = 0; i < this->grids_name.size(); ++i) {
        if(this->grids_name[i] == name) {
            return this->grids[i]->grid;
        }
    }
    //std::cout << "Wrong base mesh name: " << name << std::endl;
    return nullptr;
}

void Scene::updateTools(UITool::MeshManipulatorType tool) {
    if(this->meshManipulator) {
        delete this->meshManipulator;
        this->meshManipulator = nullptr;
    }

    BaseMesh * mesh = this->getBaseMesh(this->activeMesh);

    if(!mesh)
        return;

    if(tool == UITool::MeshManipulatorType::NONE)
        return;

    const std::vector<glm::vec3>& positions = mesh->getMeshPositions();
    this->currentTool = tool;
    if(tool == UITool::MeshManipulatorType::DIRECT) {
        this->meshManipulator = new UITool::DirectManipulator(mesh, positions);
    } else if(tool == UITool::MeshManipulatorType::POSITION) {
        this->meshManipulator = new UITool::GlobalManipulator(mesh, positions);
        //scene->updateSceneRadius();
    } else if(tool == UITool::MeshManipulatorType::ARAP) {
        this->meshManipulator = new UITool::ARAPManipulator(mesh, positions);
    } else if(tool == UITool::MeshManipulatorType::SLICE) {
        this->meshManipulator = new UITool::SliceManipulator(mesh, positions);
    }

    QObject::connect(dynamic_cast<QObject*>(this->meshManipulator), SIGNAL(needChangeCursor(UITool::CursorType)), this, SLOT(changeCursor(UITool::CursorType)));
    QObject::connect(dynamic_cast<QObject*>(this->meshManipulator), SIGNAL(needChangeCursorInPlanarView(UITool::CursorType)), this, SLOT(changeCursorInPlanarView(UITool::CursorType)));
    QObject::connect(dynamic_cast<QObject*>(this->meshManipulator), SIGNAL(needDisplayVertexInfo(std::pair<int,glm::vec3>)), this, SLOT(changeSelectedPoint(std::pair<int,glm::vec3>)));

    // To delete ?
    QObject::connect(dynamic_cast<QObject*>(this->meshManipulator), SIGNAL(needSendTetmeshToGPU()), this, SLOT(sendFirstTetmeshToGPU()));

    QObject::connect(this, SIGNAL(keyPressed(QKeyEvent*)), dynamic_cast<QObject*>(this->meshManipulator), SLOT(keyPressed(QKeyEvent*)));
    QObject::connect(this, SIGNAL(keyReleased(QKeyEvent*)), dynamic_cast<QObject*>(this->meshManipulator), SLOT(keyReleased(QKeyEvent*)));
    QObject::connect(this, SIGNAL(mousePressed(QMouseEvent*)), dynamic_cast<QObject*>(this->meshManipulator), SLOT(mousePressed(QMouseEvent*)));
    QObject::connect(this, SIGNAL(mouseReleased(QMouseEvent*)), dynamic_cast<QObject*>(this->meshManipulator), SLOT(mouseReleased(QMouseEvent*)));

    if(tool == UITool::MeshManipulatorType::POSITION) {
        QObject::connect(dynamic_cast<QObject*>(this->meshManipulator), SIGNAL(needUpdateSceneCenter()), this, SLOT(updateSceneCenter()));
    }

    if(tool == UITool::MeshManipulatorType::SLICE) {
        QObject::connect(this, SIGNAL(planesMoved(const glm::vec3&)), dynamic_cast<UITool::SliceManipulator*>(this->meshManipulator), SLOT(movePlanes(const glm::vec3&)));
    }

    // Scene->MeshManipulator->Selection
    QObject::connect(this, SIGNAL(keyPressed(QKeyEvent*)), dynamic_cast<QObject*>(&this->meshManipulator->selection), SLOT(keyPressed(QKeyEvent*)));
    QObject::connect(this, SIGNAL(keyReleased(QKeyEvent*)), dynamic_cast<QObject*>(&this->meshManipulator->selection), SLOT(keyReleased(QKeyEvent*)));
    QObject::connect(&this->meshManipulator->selection, &UITool::Selection::needToRedrawSelection, this, &Scene::redrawSelection);
    // MeshManipulator->Scene

    this->updateManipulatorRadius();

    if(tool == UITool::MeshManipulatorType::NONE || !this->getBaseMesh(this->activeMesh))
        return;

    if(tool == UITool::MeshManipulatorType::DIRECT ||
       tool == UITool::MeshManipulatorType::POSITION ) {
        this->getBaseMesh(this->activeMesh)->setNormalDeformationMethod();
    } else {
        this->getBaseMesh(this->activeMesh)->setARAPDeformationMethod();
    }

    // MeshManipulator->Scene
    //if(tool == UITool::MeshManipulatorType::FREE) {
    //    QObject::connect(this, &Scene::rayIsCasted, this, [this](const glm::vec3& origin, const glm::vec3& direction) { emit dynamic_cast<UITool::FreeManipulator*>(this->meshManipulator)->rayIsCasted(origin, direction, this->getMinTexValue(), this->getMaxTexValue(), this->computePlanePositions());});
    //}

    if(tool == UITool::MeshManipulatorType::ARAP) {
        QObject::connect(dynamic_cast<UITool::ARAPManipulator*>(this->meshManipulator), SIGNAL(needPushHandleButton()), this, SIGNAL(needPushHandleButton()));
    }

    //if(tool == UITool::MeshManipulatorType::REGISTRATION) {
    //    QObject::connect(this, &Scene::rayIsCasted, this, [this](const glm::vec3& origin, const glm::vec3& direction) { emit dynamic_cast<UITool::CompManipulator*>(this->meshManipulator)->rayIsCasted(origin, direction, this->getMinTexValue(), this->getMaxTexValue(), this->computePlanePositions());});
    //    QObject::connect(this, SIGNAL(pointIsClickedInPlanarViewer(const glm::vec3&)), dynamic_cast<UITool::CompManipulator*>(this->meshManipulator), SIGNAL(pointIsClickedInPlanarViewer(const glm::vec3&)));
    //}

    if(tool == UITool::MeshManipulatorType::SLICE) {
        //QObject::connect(dynamic_cast<UITool::SliceManipulator*>(this->meshManipulator), SIGNAL(needRedraw()), this, SLOT(computeProjection()));
        QObject::connect(dynamic_cast<UITool::SliceManipulator*>(this->meshManipulator), &UITool::SliceManipulator::needChangePointsToProject, [this](std::vector<int> selectedPoints){ this->computeProjection(selectedPoints); });
    }
}

void Scene::selectSlice(UITool::SliceOrientation sliceOrientation) {
    UITool::SliceManipulator* manipulator = dynamic_cast<UITool::SliceManipulator*>(this->meshManipulator);
    if(!manipulator) {
        std::cout << "WARNING: not the right tool" << std::endl;
        return;
    }
    manipulator->selectSlice(sliceOrientation);
}

void Scene::changeSliceToSelect(UITool::SliceOrientation sliceOrientation) {
    UITool::SliceManipulator* manipulator = dynamic_cast<UITool::SliceManipulator*>(this->meshManipulator);
    if(!manipulator) {
        std::cout << "WARNING: not the right tool" << std::endl;
        return;
    }
    manipulator->updateSliceToSelect(sliceOrientation);
}

bool Scene::isCage(const std::string& name) {
    return this->getCage(name);
}

std::vector<std::string> Scene::getAllNonTetrahedralMeshesName() {
    std::vector<std::string> res;
    for(int i = 0; i < this->meshes.size(); ++i) {
        std::string name = this->meshes[i].second;
        res.push_back(name);
    }
    return res;
}

std::vector<std::string> Scene::getAllBaseMeshesName() {
    std::vector<std::string> res;
    for(int i = 0; i < this->meshes.size(); ++i) {
        std::string name = this->meshes[i].second;
        res.push_back(name);
    }
    for(int i = 0; i < this->grids_name.size(); ++i) {
        res.push_back(this->grids_name[i]);
    }
    return res;
}

std::vector<std::string> Scene::getAllCagesName() {
    std::vector<std::string> res;
    for(int i = 0; i < this->meshes.size(); ++i) {
        std::string name = this->meshes[i].second;
        if(this->isCage(name))
            res.push_back(name);
    }
    return res;
}

std::vector<std::string> Scene::getAllGridsName() {
    return grids_name;
}

bool Scene::openCage(const std::string& name, const std::string& filename, const std::string& surfaceMeshToDeformName, const bool MVC, const glm::vec4& color) {
    this->cageToGrid.push_back(std::make_pair(name, surfaceMeshToDeformName));
    return this->openCage(name, filename, this->getBaseMesh(surfaceMeshToDeformName), MVC, color);
}

void Scene::saveMesh(const std::string& name, const std::string& filename) {
    this->getMesh(name)->saveOFF(filename.c_str());
}

bool Scene::saveActiveMesh(const std::string& filename) {
    if(!this->isGrid(this->activeMesh)) {
        this->getMesh(this->activeMesh)->saveOFF(filename.c_str());
        return true;
    } else {
        return false;
    }
}

bool Scene::saveActiveCage(const std::string& filename) {
    if(this->isCage(this->activeMesh)) {
        this->getMesh(this->activeMesh)->saveOFF(filename.c_str());
        return true;
    } else {
        return false;
    }
}

void Scene::applyCage(const std::string& name, const std::string& filename) {
    SurfaceMesh cageToApply(filename);
    this->getCage(name)->applyCage(cageToApply.getVertices());
    this->sendFirstTetmeshToGPU();
}

void Scene::redrawSelection(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3, const glm::vec4& color) {
    this->glSelection->setSelectionBB(p0, p1, p2, p3);
    this->glSelection->setColor(color);
    this->glSelection->prepare();
}

void Scene::setLightPosition(const glm::vec3& lighPosition) {
    for(int i = 0; i < this->meshes.size(); ++i) {
        this->meshes[i].first->lightPosition = lighPosition;
    }
}

void Scene::changeActiveMesh(const std::string& name) {
    this->activeMesh = name;
    this->updateSceneCenter();
    this->updateSceneRadius();
    if(this->isGrid(activeMesh)) {
        int gridIdx = this->getGridIdx(activeMesh);
        this->gridToDraw = gridIdx;
        this->sendTetmeshToGPU(gridIdx, InfoToSend(InfoToSend::VERTICES | InfoToSend::NORMALS | InfoToSend::TEXCOORD | InfoToSend::NEIGHBORS));
    } else if (this->multiGridRendering && this->isCage(activeMesh)) {
        int gridIdx = this->getGridIdxLinkToCage(activeMesh);
        if(gridIdx != -1) {
            this->gridToDraw = gridIdx;
            this->sendTetmeshToGPU(gridIdx, InfoToSend(InfoToSend::VERTICES | InfoToSend::NORMALS | InfoToSend::TEXCOORD | InfoToSend::NEIGHBORS));
        }
    }
    Q_EMIT activeMeshChanged();
    this->changeCurrentTool(this->currentTool);
    this->changeCurrentDeformationMethod(this->currentDeformMethod);
    this->shouldUpdateUserColorScales = true;
}

void Scene::changeCurrentTool(UITool::MeshManipulatorType newTool) {
    this->currentTool = newTool;
    this->updateTools(newTool);
    if(newTool == UITool::MeshManipulatorType::ARAP ||
       newTool == UITool::MeshManipulatorType::FIXED_REGISTRATION) {
        this->changeCurrentDeformationMethod(DeformMethod::ARAP);
    }
    if(newTool == UITool::MeshManipulatorType::DIRECT) {
        this->changeCurrentDeformationMethod(DeformMethod::NORMAL);
    }
}

void Scene::changeCurrentDeformationMethod(DeformMethod newDeformMethod) {
    this->currentDeformMethod = newDeformMethod;
    BaseMesh * mesh = this->getBaseMesh(this->activeMesh);
    if(mesh) {
        switch(newDeformMethod) {
            case DeformMethod::NORMAL:
                mesh->setNormalDeformationMethod();
                break;
            case DeformMethod::ARAP:
                mesh->setARAPDeformationMethod();
                break;
        }
    }
}

void Scene::changeSelectedPoint(std::pair<int, glm::vec3> selectedPoint) {
    Q_EMIT selectedPointChanged(selectedPoint);
}

bool Scene::isRightTool(const UITool::MeshManipulatorType& typeToCheck) {
    bool isRightType = (this->currentTool == typeToCheck);
    if(!isRightType)
        std::cout << "WARNING: not the right tool" << std::endl;
    return isRightType;
}

void Scene::moveTool_toggleEvenMode() { auto * toolPtr = this->getMeshTool<UITool::GlobalManipulator>(); if(toolPtr) { toolPtr->toggleEvenMode(); } };

void Scene::ARAPTool_toggleEvenMode(bool value) { auto * toolPtr = this->getMeshTool<UITool::ARAPManipulator>(); if(toolPtr) { toolPtr->toggleEvenMode(value); } };

void Scene::moveInHistory(bool backward, bool reset) {
    BaseMesh * mesh = this->getBaseMesh(this->activeMesh);
    if(mesh && mesh->history) {
        std::vector<glm::vec3> pointsBefore;
        bool success = false;
        if(reset) {
            success = mesh->history->reset(pointsBefore);
        } else {
            if(backward) {
                success = mesh->history->undo(pointsBefore);
            } else {
                success = mesh->history->redo(pointsBefore);
            }
        }
        if(success) {
            mesh->replacePoints(pointsBefore);
            this->meshManipulator->updateWithMeshVertices();
            this->updateManipulatorRadius();
            this->sendFirstTetmeshToGPU();
            this->updateSceneCenter();
        }
    } else {
        std::cout << "WARNING: the active mesh do not contain any history" << std::endl;
        return;
    }
}

void Scene::undo() {
    this->moveInHistory(true);
}

void Scene::redo() {
    this->moveInHistory(false);
}

void Scene::reset() {
    this->moveInHistory(true, true);
}

void Scene::sampleGridMapping(const std::string& fileName, const std::string& from, const std::string& to, const glm::vec3& resolution, Interpolation::Method interpolationMethod) {

    auto start = std::chrono::steady_clock::now();

    omp_set_nested(true);

    Grid * fromGrid = this->grids[this->getGridIdx(from)]->grid;
    Grid * toGrid = this->grids[this->getGridIdx(to)]->grid;

    // Space to sample
    glm::vec3 bbMinScene = fromGrid->initialMesh.bbMin;
    glm::vec3 bbMaxScene = fromGrid->initialMesh.bbMax;
    glm::vec3 fromSamplerToSceneRatio = (bbMaxScene - bbMinScene) / resolution;

    auto isInScene = [&](glm::vec3& p) {
        return (p.x > bbMinScene.x && p.y > bbMinScene.y && p.z > bbMinScene.z && p.x < bbMaxScene.x && p.y < bbMaxScene.y && p.z < bbMaxScene.z);
    };

    auto fromWorldToImage = [&](glm::vec3& p) {
        p -= bbMinScene;
        p /= fromSamplerToSceneRatio;
    };

    auto fromImageToWorld = [&](glm::vec3& p) {
        p *= fromSamplerToSceneRatio;
        p += bbMinScene;
    };

    std::vector<std::vector<uint16_t>> result;
    result.clear();
    result.resize(resolution[2]);
    for(int i = 0; i < result.size(); ++i) {
        result[i].resize(resolution[0] * resolution[1]);
        std::fill(result[i].begin(), result[i].end(), 0);
    }

    //int printOcc = 10;
    //printOcc = this->mesh.size()/printOcc;

    //#pragma omp parallel for schedule(dynamic) num_threads(fromGrid->mesh.size()/10)
    #pragma omp parallel for schedule(dynamic)
    for(int tetIdx = 0; tetIdx < fromGrid->initialMesh.mesh.size(); ++tetIdx) {
        const Tetrahedron& tet = fromGrid->initialMesh.mesh[tetIdx];
        glm::vec3 bbMin = tet.getBBMin();
        fromWorldToImage(bbMin);
        bbMin.x = std::ceil(bbMin.x) - 1;
        bbMin.y = std::ceil(bbMin.y) - 1;
        bbMin.z = std::ceil(bbMin.z) - 1;
        glm::vec3 bbMax = tet.getBBMax();
        fromWorldToImage(bbMax);
        bbMax.x = std::floor(bbMax.x) + 1;
        bbMax.y = std::floor(bbMax.y) + 1;
        bbMax.z = std::floor(bbMax.z) + 1;
        for(int k = bbMin.z; k < int(bbMax.z); ++k) {
            for(int j = bbMin.y; j < int(bbMax.y); ++j) {
                for(int i = bbMin.x; i < int(bbMax.x); ++i) {
                    glm::vec3 p(i, j, k);
                    p += glm::vec3(.5, .5, .5);

                    fromImageToWorld(p);

                    if(isInScene(p) && tet.isInTetrahedron(p)) {
                        if(fromGrid->initialMesh.getCoordInInitial(*fromGrid, p, p, tetIdx)) {

                            int insertIdx = i + j*resolution[0];
                            if(insertIdx >= result[0].size()) {
                                std::cout << "ERROR:" << std::endl;
                                std::cout << "i:" << i << std::endl;
                                std::cout << "j:" << j << std::endl;
                                std::cout << "k:" << k << std::endl;
                                std::cout << "p:" << p << std::endl;
                            }

                            //fromGrid->sampler.fromImageToSampler(p);
                            result[k][insertIdx] = toGrid->getValueFromPoint(p, Interpolation::Method::Linear);
                        }
                    }
                }
            }
        }
    }

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = end-start;
    std::cout << "Duration time: " << elapsed_seconds.count() << "s / " << elapsed_seconds.count()/60. << "m" << std::endl;

    this->writeGreyscaleTIFFImage(fileName, resolution, result);
}

void Scene::writeMapping(const std::string& fileName, const std::string& from, const std::string& to) {
    Grid * fromGrid = this->grids[this->getGridIdx(from)]->grid;
    Grid * toGrid = this->grids[this->getGridIdx(to)]->grid;

    glm::ivec3 fromDimensions = fromGrid->sampler.getSamplerDimension();
    std::vector<std::vector<uint16_t>> img = std::vector<std::vector<uint16_t>>(fromDimensions.z, std::vector<uint16_t>(fromDimensions.x * fromDimensions.y, 0.));

    auto start = std::chrono::steady_clock::now();
    for(int k = 0; k < fromDimensions.z; ++k) {
        std::cout << "Loading: " << (float(k)/float(fromDimensions.z)) * 100. << "%" << std::endl;
        #pragma omp parallel for schedule(dynamic)
        for(int i = 0; i < fromDimensions.x; ++i) {
            for(int j = 0; j < fromDimensions.y; ++j) {
                glm::vec3 inputPointInFromGrid(i, j, k);
                glm::vec3 result;
                fromGrid->sampler.fromImageToSampler(inputPointInFromGrid);

                int tetIdx = -1;
                bool ptIsInInitial = fromGrid->initialMesh.getCoordInInitialOut(*fromGrid, inputPointInFromGrid, result, tetIdx);
                if(!ptIsInInitial) {
                    img[k][i + j*fromDimensions.x] = 0;
                } else {
                    ptIsInInitial = toGrid->getCoordInInitial(toGrid->initialMesh, result, result, tetIdx);
                    if(!ptIsInInitial) {
                        img[k][i + j*fromDimensions.x] = 0;
                    } else {
                        toGrid->sampler.fromSamplerToImage(result);
                        img[k][i + j*fromDimensions.x] = toGrid->getValueFromPoint(result);
                    }
                }
            }
        }
        auto end = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed_seconds = end-start;
        std::cout << "Duration time: " << elapsed_seconds.count() << "s / " << elapsed_seconds.count()/60. << "m" << std::endl;
    }

    this->writeGreyscaleTIFFImage(fileName, fromDimensions, img);
}

void Scene::writeDeformedImage(const std::string& filename, const std::string& gridName, bool useColorMap, ResolutionMode resolution) {
    Grid * fromGrid = this->grids[this->getGridIdx(gridName)]->grid;
    if(useColorMap)
        this->writeDeformedImageGeneric(filename, gridName, (Image::ImageDataType::Unsigned | Image::ImageDataType::Bit_16), useColorMap, resolution);
    else
        this->writeDeformedImageGeneric(filename, gridName, fromGrid->sampler.getInternalDataType(), useColorMap, resolution);
}

void Scene::writeDeformedImageGeneric(const std::string& filename, const std::string& gridName, Image::ImageDataType imgDataType, bool useColorMap, ResolutionMode resolution) {
    if(imgDataType == (Image::ImageDataType::Unsigned | Image::ImageDataType::Bit_8)) {
        this->writeDeformedImageTemplated<uint8_t>(filename, gridName, 8, imgDataType, useColorMap, resolution);
    } else if(imgDataType == (Image::ImageDataType::Unsigned | Image::ImageDataType::Bit_16)) {
        this->writeDeformedImageTemplated<uint16_t>(filename, gridName, 16, imgDataType, useColorMap, resolution);
    } else if(imgDataType == (Image::ImageDataType::Unsigned | Image::ImageDataType::Bit_32)) {
        this->writeDeformedImageTemplated<uint32_t>(filename, gridName, 32, imgDataType, useColorMap, resolution);
    } else if(imgDataType == (Image::ImageDataType::Unsigned | Image::ImageDataType::Bit_64)) {
        this->writeDeformedImageTemplated<uint64_t>(filename, gridName, 64, imgDataType, useColorMap, resolution);
    } else if(imgDataType == (Image::ImageDataType::Signed | Image::ImageDataType::Bit_8)) {
        this->writeDeformedImageTemplated<int8_t>(filename, gridName, 8, imgDataType, useColorMap, resolution);
    } else if(imgDataType == (Image::ImageDataType::Signed | Image::ImageDataType::Bit_16)) {
        this->writeDeformedImageTemplated<int16_t>(filename, gridName, 16, imgDataType, useColorMap, resolution);
    } else if(imgDataType == (Image::ImageDataType::Signed | Image::ImageDataType::Bit_32)) {
        this->writeDeformedImageTemplated<int32_t>(filename, gridName, 32, imgDataType, useColorMap, resolution);
    } else if(imgDataType == (Image::ImageDataType::Signed | Image::ImageDataType::Bit_64)) {
        this->writeDeformedImageTemplated<int64_t>(filename, gridName, 64, imgDataType, useColorMap, resolution);
    } else if(imgDataType == (Image::ImageDataType::Floating | Image::ImageDataType::Bit_32)) {
        this->writeDeformedImageTemplated<float>(filename, gridName, 32, imgDataType, useColorMap, resolution);
    } else if(imgDataType == (Image::ImageDataType::Floating | Image::ImageDataType::Bit_64)) {
        this->writeDeformedImageTemplated<double>(filename, gridName, 64, imgDataType, useColorMap, resolution);
    } 
}

template<typename DataType>
void Scene::writeDeformedImageTemplated(const std::string& filename, const std::string& gridName, int bit, Image::ImageDataType dataType, bool useColorMap, ResolutionMode resolution) {
    // To expose as parameters
    bool smallFile = true;
    int cacheSize = 2;
    bool useCustomColor = useColorMap;
    //glm::ivec3 imageSize = glm::vec3(60, 264, 500);
    glm::ivec3 imageSize = glm::vec3(0, 0, 0);
    //ResolutionMode resolution = ResolutionMode::FULL_RESOLUTION;

    auto start = std::chrono::steady_clock::now();

    Grid * fromGrid = this->grids[this->getGridIdx(gridName)]->grid;
    glm::vec3 fromImageToCustomImage = glm::vec3(0., 0., 0.);

    auto fromWorldToImage = [&](glm::vec3& p, bool ceil) {
        p -= fromGrid->bbMin;
        for(int i = 0; i < 3; ++i) {
            if(ceil)
                p[i] = std::ceil(p[i]/fromGrid->getVoxelSize(resolution)[i]); 
            else
                p[i] = std::floor(p[i]/fromGrid->getVoxelSize(resolution)[i]); 
        }
        p /= fromImageToCustomImage;
    };

    auto getWorldCoordinates = [&](glm::vec3& p) {
        for(int i = 0; i < 3; ++i) {
            p[i] = (float(std::ceil(p[i] * fromImageToCustomImage[i])) + 0.5) * fromGrid->getVoxelSize(resolution)[i];
        }
        p += fromGrid->bbMin;
    };

    glm::vec3 worldSize = fromGrid->getDimensions();
    glm::vec3 voxelSize = fromGrid->getVoxelSize(resolution);

    glm::ivec3 n(0, 0, 0);
    for(int i = 0 ; i < 3 ; i++) {
        n[i] = std::ceil(fabs(worldSize[i])/voxelSize[i]);
    }

    if(imageSize == glm::ivec3(0., 0., 0.))
        imageSize = n;

    fromImageToCustomImage = glm::vec3(n) / glm::vec3(imageSize);

    TinyTIFFWriterFile * tif = nullptr;
    if(useCustomColor) {
        tif = TinyTIFFWriter_open(filename.c_str(), 8, TinyTIFFWriter_UInt, 3, imageSize[0], imageSize[1], TinyTIFFWriter_RGB);
    } else {
        if(dataType & Image::ImageDataType::Unsigned)
            tif = TinyTIFFWriter_open(filename.c_str(), bit, TinyTIFFWriter_UInt, 1, imageSize[0], imageSize[1], TinyTIFFWriter_Greyscale);
        else if(dataType & Image::ImageDataType::Signed)
            tif = TinyTIFFWriter_open(filename.c_str(), bit, TinyTIFFWriter_Int, 1, imageSize[0], imageSize[1], TinyTIFFWriter_Greyscale);
        else if(dataType & Image::ImageDataType::Floating)
            tif = TinyTIFFWriter_open(filename.c_str(), bit, TinyTIFFWriter_Float, 1, imageSize[0], imageSize[1], TinyTIFFWriter_Greyscale);
        else
            std::cout << "WARNING: image data type no take in charge to export" << std::endl;
    }

    if(tif == nullptr)
        return;

    std::vector<std::vector<DataType>> img = std::vector<std::vector<DataType>>(imageSize.z, std::vector<DataType>(imageSize.x * imageSize.y, 0.));

    std::vector<std::vector<uint8_t>> img_color;

    std::vector<bool> data;
    std::vector<glm::vec3> data_color;

    if(useCustomColor) {
        img_color = std::vector<std::vector<uint8_t>>(imageSize.z, std::vector<uint8_t>(imageSize.x * imageSize.y * 3, 0));

        auto upperGrid = this->grids[this->getGridIdx(gridName)];
        float maxValue = upperGrid->maxValue;

        for(int i = 0; i <= maxValue; ++i) {
            data.push_back(false);
            data_color.push_back(glm::vec3(0., 0., 0.));
        }
        for(int i = 0; i < upperGrid->visu.size(); ++i) {
            for(int j = upperGrid->visu[i].first; j <= upperGrid->visu[i].second; ++j) {
                if(j < data.size()) {
                    data[j] = true;
                    data_color[j] = upperGrid->visu_color[i];
                }
            }
        }
    }

    int cacheMaxNb = std::floor(fromGrid->sampler.image->tiffImageReader->imgResolution.z / float(cacheSize));
    std::map<int, std::vector<DataType>> cache;

    if(smallFile)
        for(int i = 0; i < fromGrid->sampler.image->tiffImageReader->imgResolution.z; ++i)
            fromGrid->sampler.image->tiffImageReader->getImage<DataType>(i, cache[i], {glm::vec3(0., 0., 0.), fromGrid->sampler.image->tiffImageReader->imgResolution});

    #pragma omp parallel for schedule(static) if(smallFile)
    for(int tetIdx = 0; tetIdx < fromGrid->mesh.size(); ++tetIdx) {
        std::cout << "Tet: " << tetIdx << "/" << fromGrid->mesh.size() << std::endl;
        const Tetrahedron& tet = fromGrid->mesh[tetIdx];
        glm::vec3 bbMin = tet.getBBMin();
        glm::vec3 bbMax = tet.getBBMax();
        fromWorldToImage(bbMin, false);
        fromWorldToImage(bbMax, true);
        int X = bbMax.x;
        int Y = bbMax.y;
        int Z = bbMax.z;
        for(int k = bbMin.z; k < Z; ++k) {
            for(int j = bbMin.y; j < Y; ++j) {
                for(int i = bbMin.x; i < X; ++i) {
                    glm::vec3 p(i, j, k);
                    getWorldCoordinates(p);
                    if(tet.isInTetrahedron(p)) {
                        if(fromGrid->getCoordInInitial(fromGrid->initialMesh, p, p, tetIdx)) {
                            int insertIdx = i + j*imageSize[0];

                            int imgIdxLoad = std::floor(p.z);
                            int idxLoad = std::floor(p.x) + std::floor(p.y) * fromGrid->sampler.image->tiffImageReader->imgResolution.x;

                            if(img[k][insertIdx] == 0) {

                                bool isInBBox = true;
                                for(int l = 0; l < 3; ++l) {
                                    if(p[l] < 0. || p[l] >= fromGrid->sampler.image->tiffImageReader->imgResolution[l])
                                        isInBBox = false;
                                }

                                if(isInBBox) {
                                    bool imgAlreadyLoaded = cache.find(imgIdxLoad) != cache.end();
                                    if(!imgAlreadyLoaded) {
                                        if(cache.size() > cacheMaxNb) {
                                            cache.erase(cache.begin());
                                        }
                                        fromGrid->sampler.image->tiffImageReader->getImage<DataType>(imgIdxLoad, cache[imgIdxLoad], {glm::vec3(0., 0., 0.), fromGrid->sampler.image->tiffImageReader->imgResolution});
                                    }
                                    if(useCustomColor) {
                                        uint16_t value = 0; 
                                        value = cache[imgIdxLoad][idxLoad];
                                        if(data[value]) {
                                            glm::vec3 color = data_color[value];
                                            insertIdx *= 3;
                                            img_color[k][insertIdx] = static_cast<uint8_t>(color.r * 255.);
                                            img_color[k][insertIdx+1] = static_cast<uint8_t>(color.g * 255.);
                                            img_color[k][insertIdx+2] = static_cast<uint8_t>(color.b * 255.);
                                        }
                                    } else {
                                        img[k][insertIdx] = cache[imgIdxLoad][idxLoad];
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    //this->writeGreyscaleTIFFImage(filename, n, img);

    if(useCustomColor) {
        for(int i = 0; i < img_color.size(); ++i) {
            TinyTIFFWriter_writeImage(tif, img_color[i].data());
        }
    } else {
        for(int i = 0; i < img.size(); ++i) {
            TinyTIFFWriter_writeImage(tif, img[i].data());
        }
    }
    TinyTIFFWriter_close(tif);
    std::cout << "Destination: " << filename << std::endl;
    std::cout << "Save sucessfull" << std::endl;

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = end-start;
    std::cout << "Duration time: " << elapsed_seconds.count() << "s / " << elapsed_seconds.count()/60. << "m" << std::endl;
}

void Scene::writeDeformedImageLowRes(const std::string& filename, const std::string& gridName) {
    ResolutionMode resolution = ResolutionMode::SAMPLER_RESOLUTION;
    auto start = std::chrono::steady_clock::now();
    Grid * fromGrid = this->grids[this->getGridIdx(gridName)]->grid;

    auto fromWorldToImage = [&](glm::vec3& p, bool ceil) {
        p -= fromGrid->bbMin;
        for(int i = 0; i < 3; ++i) {
            if(ceil)
                p[i] = std::ceil(p[i]/fromGrid->getVoxelSize(resolution)[i]); 
            else
                p[i] = std::floor(p[i]/fromGrid->getVoxelSize(resolution)[i]); 
        }
    };

    auto getWorldCoordinates = [&](glm::vec3& p) {
        for(int i = 0; i < 3; ++i) {
            p[i] = (p[i]+0.5) * fromGrid->getVoxelSize(resolution)[i];
        }
        p += fromGrid->bbMin;
    };

    // STEP1: compute the deformed voxel grid size
    glm::vec3 worldSize = fromGrid->getDimensions();
    glm::vec3 voxelSize = fromGrid->getVoxelSize(resolution);

    glm::ivec3 n(0, 0, 0);
    for(int i = 0 ; i < 3 ; i++) {
        n[i] = std::ceil(fabs(worldSize[i])/voxelSize[i])+1;
    }

    std::vector<std::vector<uint8_t>> img = std::vector<std::vector<uint8_t>>(n.z, std::vector<uint8_t>(n.x * n.y, 0.));

    #pragma omp parallel for schedule(static)
    for(int tetIdx = 0; tetIdx < fromGrid->mesh.size(); ++tetIdx) {
        std::cout << "Tet: " << tetIdx << "/" << fromGrid->mesh.size() << std::endl;
        const Tetrahedron& tet = fromGrid->mesh[tetIdx];
        glm::vec3 bbMin = tet.getBBMin();
        glm::vec3 bbMax = tet.getBBMax();
        fromWorldToImage(bbMin, false);
        fromWorldToImage(bbMax, true);
        int X = bbMax.x;
        int Y = bbMax.y;
        int Z = bbMax.z;
        for(int k = bbMin.z; k < Z; ++k) {
            for(int j = bbMin.y; j < Y; ++j) {
                for(int i = bbMin.x; i < X; ++i) {
                    glm::vec3 p(i, j, k);
                    getWorldCoordinates(p);
                    //if(tet.isInTetrahedron(p)) {
                        if(fromGrid->getCoordInInitial(fromGrid->initialMesh, p, p, tetIdx)) {
                            int insertIdx = i + j*n[0];
                            if(insertIdx >= img[0].size()) {
                                std::cout << "ERROR:" << std::endl;
                                std::cout << "i:" << i << std::endl;
                                std::cout << "j:" << j << std::endl;
                                std::cout << "k:" << k << std::endl;
                                std::cout << "p:" << p << std::endl;
                            }

                            if(img[k][insertIdx] == 0) {
                                //img[k][insertIdx] = fromGrid->getValueFromPoint(p, Interpolation::Method::Linear);
                                img[k][insertIdx] = static_cast<uint8_t>(fromGrid->getValueFromPoint(p));
                            }
                        }
                    //}
                }
            }
        }
    }

    //this->writeGreyscaleTIFFImage(filename, n, img);

    TinyTIFFWriterFile * tif = TinyTIFFWriter_open(filename.c_str(), 8, TinyTIFFWriter_UInt, 1, n[0], n[1], TinyTIFFWriter_Greyscale);
    for(int i = 0; i < img.size(); ++i) {
        TinyTIFFWriter_writeImage(tif, img[i].data());
    }
    TinyTIFFWriter_close(tif);
    std::cout << "Destination: " << filename << std::endl;
    std::cout << "Save sucessfull" << std::endl;

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = end-start;
    std::cout << "Duration time: " << elapsed_seconds.count() << "s / " << elapsed_seconds.count()/60. << "m" << std::endl;

}

glm::vec3 Scene::getTransformedPoint(const glm::vec3& inputPoint, const std::string& from, const std::string& to) {
    glm::vec3 result = glm::vec3(0., 0., 0.);

    Grid * fromGrid = this->grids[this->getGridIdx(from)]->grid;
    Grid * toGrid = this->grids[this->getGridIdx(to)]->grid;

    // From initial to deformed
    glm::vec3 inputPointInFromGrid = inputPoint;
    std::cout << "-------------" << std::endl;
    std::cout << "Input point: " << inputPoint << std::endl;
    std::cout << "From [" << from << "].image to [" << from << "].sampler: " << inputPointInFromGrid;
    fromGrid->sampler.fromImageToSampler(inputPointInFromGrid);
    std::cout << " -> " << inputPointInFromGrid << std::endl;

    bool ptIsInInitial = fromGrid->initialMesh.getCoordInInitial(*fromGrid, inputPointInFromGrid, result);
    std::cout << "From [" << from << "].initial to [" << from << "].deformed: " << inputPointInFromGrid << " -> " << result << std::endl;
    if(!ptIsInInitial) {
        std::cout << "Input point isn't in the original image" << std::endl;
    }

    std::cout << "From [" << from  << "].deformed to [" << to << "].deformed: manually done" << std::endl;

    std::cout << "From [" << to  << "].deformed to [" << to << "].initial: " << result;
    ptIsInInitial = toGrid->getCoordInInitial(toGrid->initialMesh, result, result);
    std::cout << " -> " << result << std::endl;
    if(!ptIsInInitial) {
        std::cout << "Input point isn't in the original image" << std::endl;
    }

    std::cout << "From [" << to << "].sampler to [" << to << "].image: " << result;
    toGrid->sampler.fromSamplerToImage(result);
    std::cout << " -> " << result << std::endl;

    std::cout << "-------------" << std::endl;
    std::cout << "From [" << from  << "] to [" << to << "]grid: " << inputPoint << " -> " << result << std::endl;
    std::cout << "-------------" << std::endl;

    return result;
}

void Scene::getDeformation(const std::string& gridNameValues, const std::string& gridNameSample, std::vector<std::vector<uint16_t>>& data) {
    Grid * grid = this->grids[this->getGridIdx(gridNameSample)]->grid;

    const glm::vec3 bbMin = grid->bbMin;
    const glm::vec3 bbMax = grid->bbMax;
    const glm::vec3 imgSize = grid->getResolution();

    this->grids[this->getGridIdx(gridNameValues)]->grid->sampleGridValues(std::make_pair(grid->bbMin, grid->bbMax), imgSize, data);
}

void Scene::getValues(const std::string& gridName, const std::pair<glm::vec3, glm::vec3>& area, const glm::vec3& resolution, std::vector<std::vector<uint16_t>>& data, Interpolation::Method interpolationMethod) {
    this->grids[this->getGridIdx(gridName)]->grid->sampleGridValues(area, resolution, data, interpolationMethod);
}

void Scene::getValues(const std::string& gridName, const glm::vec3& slice, const std::pair<glm::vec3, glm::vec3>& area, const glm::vec3& resolution, std::vector<uint16_t>& data, Interpolation::Method interpolationMethod) {
    this->grids[this->getGridIdx(gridName)]->grid->sampleSliceGridValues(slice, area, resolution, data, interpolationMethod);
}

void Scene::writeDeformation(const std::string& filename, const std::string& gridNameValues, const std::string& gridNameSample) {
    std::vector<std::vector<uint16_t>> data;
    this->getDeformation(gridNameValues, gridNameSample, data);
    this->writeGreyscaleTIFFImage(filename, this->getGridImgSize(gridNameSample), data);
}

void Scene::writeGreyscaleTIFFImage(const std::string& filename, const glm::vec3& imgDimensions, const std::vector<std::vector<uint16_t>>& data) {
    TinyTIFFWriterFile * tif = TinyTIFFWriter_open(filename.c_str(), 16, TinyTIFFWriter_UInt, 1, imgDimensions[0], imgDimensions[1], TinyTIFFWriter_Greyscale);
    for(int img = 0; img < data.size(); ++img) {
        TinyTIFFWriter_writeImage(tif, data[img].data());
    }
    TinyTIFFWriter_close(tif);
    std::cout << "Destination: " << filename << std::endl;
    std::cout << "Save sucessfull" << std::endl;
}

void Scene::writeImageWithPoints(const std::string& filename, const std::string& image, std::vector<glm::vec3>& points) {
    Grid * grid = this->grids[this->getGridIdx(image)]->grid;

    glm::ivec3 imgDimensions = grid->sampler.getImageDimensions();

    TinyTIFFWriterFile * tif = TinyTIFFWriter_open(filename.c_str(), 16, TinyTIFFWriter_UInt, 1, imgDimensions[0], imgDimensions[1], TinyTIFFWriter_Greyscale);
    std::vector<uint16_t> data;
    data.resize(imgDimensions[0] * imgDimensions[1]);

    auto start = std::chrono::steady_clock::now();
    for(int k = 0; k < imgDimensions[2]; ++k) {
        std::cout << "Loading: " << (float(k)/float(imgDimensions[2])) * 100. << "%" << std::endl;
        #pragma omp parallel for
        for(int j = 0; j < imgDimensions[1]; ++j) {
            for(int i = 0; i < imgDimensions[0]; ++i) {
                glm::vec3 p(i, j, k);
                int insertIdx = i + j*imgDimensions[0];
                bool toHighlight = false;
                for(const glm::vec3& pt : points) {
                    if(glm::distance(pt, p) < 5.) {
                        toHighlight = true;
                    }
                }
                if(toHighlight) {
                    data[insertIdx] = 0;
                } else {
                    data[insertIdx] = grid->getValueFromPoint(p);
                }
            }
        }
        TinyTIFFWriter_writeImage(tif, data.data());
        auto end = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed_seconds = end-start;
        std::cout << "Duration time: " << elapsed_seconds.count() << "s / " << elapsed_seconds.count()/60. << "m" << std::endl;
    }
    TinyTIFFWriter_close(tif);

    std::cout << "Save sucessfull" << std::endl;
}

void Scene::clear() {
    this->updateTools(UITool::MeshManipulatorType::NONE);
    this->grids_name.clear();
    this->grids.clear();
    this->meshes.clear();
    this->graph_meshes.clear();
    gridToDraw = -1;
}

glm::vec3 Scene::getGridImgSize(const std::string& name) {
    return this->grids[this->getGridIdx(name)]->grid->getResolution();
}

glm::vec3 Scene::getGridVoxelSize(const std::string &name, ResolutionMode resolution) {
    return this->grids[this->getGridIdx(name)]->grid->getVoxelSize(resolution);
}

std::pair<uint16_t, uint16_t> Scene::getGridMinMaxValues(const std::string& name) {
    return std::make_pair(this->grids[this->getGridIdx(name)]->minValue, this->grids[this->getGridIdx(name)]->maxValue);
}

std::pair<uint16_t, uint16_t> Scene::getGridMinMaxValues() {
    if(gridToDraw != -1)
        return std::make_pair(this->grids[this->gridToDraw]->minValue, this->grids[this->gridToDraw]->maxValue);
    return std::make_pair(0, 0);
}

std::vector<bool> Scene::getGridUsageValues(int minValue) {
    std::vector<int> histogram = this->grids[this->gridToDraw]->grid->sampler.getHistogram(this->grids[this->gridToDraw]->maxValue+1);
    if(gridToDraw != -1) {
        std::vector<bool> usage;
        usage.reserve(histogram.size());
        for(auto& value : histogram) {
            if(value >= minValue) {
                usage.push_back(true);
            } else {
                usage.push_back(false);
            }
        }
        return usage;
    }
    return std::vector<bool>{false};
}

template<typename MeshToolType>
MeshToolType* Scene::getMeshTool() { return dynamic_cast<MeshToolType*>(this->meshManipulator); };

void Scene::setGridsToDraw(std::vector<int> indices) {
    this->gridsToDraw = indices;
}

void Scene::setSortingRendering(bool value) {
    this->sortingRendering = value;
}

void Scene::setMultiGridRendering(bool value) {
    if(this->multiGridRendering) {
        glDeleteFramebuffers(1, &this->frameBuffer);
        glDeleteTextures(1, &this->dualRenderingTexture);
        glDeleteTextures(1, &this->frameDepthBuffer);
    }
    this->multiGridRendering = value;
    if(this->isCage(activeMesh)) {
        int gridIdx = this->getGridIdxLinkToCage(activeMesh);
        if(gridIdx != -1) {
            this->gridToDraw = gridIdx;
            this->sendTetmeshToGPU(gridIdx, InfoToSend(InfoToSend::VERTICES | InfoToSend::NORMALS | InfoToSend::TEXCOORD | InfoToSend::NEIGHBORS));
        }
    }
};

void Scene::setDrawOnlyBoundaries(bool value) {
    std::cout << "Set draw only boundaries: " << value << std::endl;
    this->drawOnlyBoundaries = value;
}

void Scene::setBlendFirstPass(float value) {
    this->blendFirstPass = value;
}

void Scene::setRenderSize(int h, int w) {
    this->h = h;
    this->w = w;
}

void Scene::resetRanges() {
   if(this->gridToDraw == -1)
       return;
   auto& grid = this->grids[this->gridToDraw];
   grid->visu.clear();
   grid->visu_color.clear();
   grid->visu_visi.clear();
   this->newSHADERS_updateUBOData();
}

void Scene::addRange(uint16_t min, uint16_t max, glm::vec3 color, bool visible, bool updateUBO) {
   if(this->gridToDraw == -1)
       return;
   auto& grid = this->grids[this->gridToDraw];
   grid->visu.push_back(std::make_pair(min, max));
   grid->visu_color.push_back(color);
   grid->visu_visi.push_back(visible);
   if(updateUBO)
       this->newSHADERS_updateUBOData();
}

void Scene::getRanges(std::vector<std::pair<uint16_t, uint16_t>>& ranges) {
   if(this->gridToDraw == -1)
       return;
   auto& grid = this->grids[this->gridToDraw];
   ranges = grid->visu;
}

void Scene::getRangesColor(std::vector<glm::vec3>& colors) {
   if(this->gridToDraw == -1)
       return;
   auto& grid = this->grids[this->gridToDraw];
   colors = grid->visu_color;
}

void Scene::getRangesVisu(std::vector<bool>& visu) {
   if(this->gridToDraw == -1)
       return;
   auto& grid = this->grids[this->gridToDraw];
   visu = grid->visu_visi;
}

//void Scene::removeRange(const std::string &gridName, uint16_t min, uint16_t max) {
//   if(this->gridToDraw == -1)
//       return;
//   auto& grid = this->grids[this->getGridIdx(gridName)];
//   for(int i = 0; i < grid->visu.size(); ++i) {
//       if(grid->visu[i].first == min && grid->visu[i].first == max) {
//           grid->visu.erase(grid->visu.begin()+i);
//           break;
//       }
//   }
//   this->newSHADERS_updateUBOData();
//}

bool Scene::hasTwoOrMoreGrids() {
    return grids.size() >= 2;
}

void Scene::updateManipulatorRadius() {
    Q_EMIT sceneRadiusOutOfDate();
}

void Scene::computeProjection(const std::vector<int>& vertexIndices) {
    int knnMaxNode = 15;

    std::vector<glm::vec3> newPositions;

    struct KnnNode {
        int indice;
        float distance;

        KnnNode(): indice(-1), distance(std::numeric_limits<float>::max()) {}

        bool operator<(const KnnNode& other) const {
            return (distance < other.distance);
        }

        bool operator>(const KnnNode& other) const {
            return (distance > other.distance);
        }
    };

    BaseMesh * meshToProject = this->meshes[0].first;
    BaseMesh * mesh = this->meshes[1].first;
    std::vector<glm::vec3> positions = mesh->getVertices();
    std::vector<glm::vec3> normals = mesh->verticesNormals;
    for(int k = 0; k < vertexIndices.size(); ++k) {
        int vertexIdx = vertexIndices[k];

        glm::vec3 inputPoint = meshToProject->getVertice(vertexIdx);
        std::vector<KnnNode> knn(knnMaxNode);

        for(int i = 0; i < positions.size(); ++i) {
            if(i != vertexIdx) {
                float distance = glm::distance(inputPoint, positions[i]);
                if(distance < knn.back().distance) {
                    knn.back().indice = i;
                    knn.back().distance = distance;
                    std::sort(knn.begin(), knn.end());
                }
            }
        }

        std::vector<int> knn_indices(knnMaxNode);
        std::vector<float> knn_distances(knnMaxNode);
        for(int i = 0; i < knn.size(); ++i) {
            knn_indices[i] = knn[i].indice;
            knn_distances[i] = knn[i].distance * knn[i].distance;
        }

        ProjectedPoint res;
        res = apss(inputPoint,
                   positions,
                   normals,
                   knn_indices,// nth closest points from inputPoint
                   knn_distances);// same but distances

        newPositions.push_back(res.position);
    }
    meshToProject->replacePoints(vertexIndices, newPositions);

    dynamic_cast<UITool::SliceManipulator*>(this->meshManipulator)->updateWithMeshVertices();
    dynamic_cast<UITool::SliceManipulator*>(this->meshManipulator)->moveGuizmo();

    //ARAPMethod * deformer = dynamic_cast<ARAPMethod*>(meshToProject->meshDeformer);
    //if(!deformer) {
    //    std::cout << "WARNING: ARAP manipulator can be used only with the ARAP deformer !" << std::endl;
    //    return;
    //}

    //std::vector<glm::vec3> newFullPositions = meshToProject->getVertices();
    //for(int i = 0; i < vertexIndices.size(); ++i) {
    //    newFullPositions[vertexIndices[i]] = newPositions[i];
    //}

    //std::vector<Vec3D<float>> ptsAsVec3D;
    //for(int i = 0; i < newFullPositions.size(); ++i) {
    //    glm::vec3 pt = newFullPositions[i];
    //    ptsAsVec3D.push_back(Vec3D(pt[0], pt[1], pt[2]));
    //}
    //deformer->arap.compute_deformation(ptsAsVec3D);

    //for(int i = 0; i < newFullPositions.size(); ++i)
    //    newFullPositions[i] = glm::vec3(ptsAsVec3D[i][0], ptsAsVec3D[i][1], ptsAsVec3D[i][2]);


    //meshToProject->useNormal = true;
    //meshToProject->movePoints(newFullPositions);
    //meshToProject->useNormal = false;

    //this->meshManipulator->updateWithMeshVertices();
    //this->updateManipulatorRadius();
    //this->sendFirstTetmeshToGPU();
    //this->updateSceneCenter();
}
