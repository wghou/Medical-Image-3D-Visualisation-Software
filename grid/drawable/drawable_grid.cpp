#include "drawable_grid.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <memory>

DrawableGrid::DrawableGrid(GridGLView::Ptr grid): gl(nullptr) {
    this->displayTetmesh = false;

    this->program_VolumetricViewer = 0;
    this->tex_ColorScaleGrid			= 0;
    this->tex_ColorScaleGridAlternate = 0;
    this->tex_ColorScaleGrid = 0;
    this->grid = grid;
    this->blendFirstPass = 1.;
    this->drawOnlyBoundaries = true;

    std::cout << "Create drawable grid" << std::endl;
};

void DrawableGrid::recompileShaders() {
    std::cout << "Compile shaders of drawable grid" << std::endl;
    GLuint newVolumetricProgram	 = this->compileShaders("../shaders/transfer_mesh.vert", "../shaders/transfer_mesh.geom", "../shaders/transfer_mesh.frag");
    if (newVolumetricProgram) {
        gl->glDeleteProgram(this->program_VolumetricViewer);
        this->program_VolumetricViewer = newVolumetricProgram;
    }
}

void DrawableGrid::initializeGL(ShaderCompiler::GLFunctions *functions) {
    std::cout << "Initialize drawable grid" << std::endl;
    this->gl = functions;
    this->shaderCompiler = std::make_unique<ShaderCompiler>(functions);
    this->recompileShaders();
    this->createBuffers();
    this->generateColorScales();
}

void DrawableGrid::generateColorScales() {
    TextureUpload colorScaleUploadParameters;

    int maximumTextureSize = 0;
    glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &maximumTextureSize);
    std::size_t textureSize = maximumTextureSize / 2u;
    float textureSize_f		= static_cast<float>(maximumTextureSize / 2u);

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

void DrawableGrid::createBuffers() {
    glGenTextures(1, &this->dualRenderingTexture);

    glBindTexture(GL_TEXTURE_2D, this->dualRenderingTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2024, 1468, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glGenTextures(1, &this->frameDepthBuffer);
    glBindTexture(GL_TEXTURE_2D, this->frameDepthBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0,GL_DEPTH_COMPONENT32, 2024, 1468, 0,GL_DEPTH_COMPONENT, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    gl->glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, this->frameDepthBuffer, 0);

    // Set "renderedTexture" as our colour attachement #0
    gl->glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, this->dualRenderingTexture, 0);

    // Set the list of draw buffers.
    GLenum DrawBuffers[1] = {GL_COLOR_ATTACHMENT0};
    gl->glDrawBuffers(1, DrawBuffers); // "1" is the size of DrawBuffers

    // Always check that our framebuffer is ok
    if(gl->glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "WARNING: framebuffer doesn't work !!" << std::endl;
    else
        std::cout << "Framebuffer works perfectly :) !!" << std::endl;

}

GLuint DrawableGrid::compileShaders(std::string _vPath, std::string _gPath, std::string _fPath) {
    gl->glUseProgram(0);
    this->shaderCompiler->reset();
    this->shaderCompiler->pragmaReplacement_file("include_color_shader", "../shaders/colorize_new_flow.glsl");
    this->shaderCompiler->vertexShader_file(_vPath).geometryShader_file(_gPath).fragmentShader_file(_fPath);
    if (this->shaderCompiler->compileShaders()) {
        return this->shaderCompiler->programName();
    }
    std::cerr << this->shaderCompiler->errorString() << '\n';
    return 0;
}


void DrawableGrid::prepareUniforms(GLfloat* mvMat, GLfloat* pMat, glm::vec3 camPos, glm::vec3 planePosition, glm::vec3 planeDirection, bool drawFront) {
    /// @brief Shortcut for glGetUniform, since this can result in long lines.
    auto getUniform = [&](const char* name) -> GLint {
        GLint g = gl->glGetUniformLocation(program_VolumetricViewer, name);
        return g;
    };

    std::size_t tex = 0;
    glActiveTexture(GL_TEXTURE0 + tex);
    glBindTexture(GL_TEXTURE_2D, grid->volumetricMesh.vertexPositions);
    gl->glUniform1i(getUniform("vertices_translations"), tex);
    tex++;

    glActiveTexture(GL_TEXTURE0 + tex);
    glBindTexture(GL_TEXTURE_2D, grid->volumetricMesh.faceNormals);
    gl->glUniform1i(getUniform("normals_translations"), tex);
    tex++;

    glActiveTexture(GL_TEXTURE0 + tex);
    glBindTexture(GL_TEXTURE_2D, grid->volumetricMesh.visibilityMap);
    gl->glUniform1i(getUniform("visibility_texture"), tex);
    tex++;

    glActiveTexture(GL_TEXTURE0 + tex);
    glBindTexture(GL_TEXTURE_2D, grid->volumetricMesh.textureCoordinates);
    gl->glUniform1i(getUniform("texture_coordinates"), tex);
    tex++;

    glActiveTexture(GL_TEXTURE0 + tex);
    glBindTexture(GL_TEXTURE_2D, grid->volumetricMesh.neighborhood);
    gl->glUniform1i(getUniform("neighbors"), tex);
    tex++;

    glActiveTexture(GL_TEXTURE0 + tex);
    glBindTexture(GL_TEXTURE_3D, grid->gridTexture);
    gl->glUniform1i(getUniform("texData"), tex);
    tex++;

    glActiveTexture(GL_TEXTURE0 + tex);
    glBindTexture(GL_TEXTURE_2D, this->tex_ColorScaleGrid);
    gl->glUniform1i(getUniform("visiblity_map"), tex);
    tex++;

    glActiveTexture(GL_TEXTURE0 + tex);
    glBindTexture(GL_TEXTURE_2D, this->tex_ColorScaleGridAlternate);
    gl->glUniform1i(getUniform("visiblity_map_alternate"), tex);
    tex++;

    glActiveTexture(GL_TEXTURE0 + tex);
    glBindTexture(GL_TEXTURE_2D, this->dualRenderingTexture);
    gl->glUniform1i(getUniform("firstPass_texture"), tex);
    tex++;

    glActiveTexture(GL_TEXTURE0 + tex);
    glBindTexture(GL_TEXTURE_2D, this->frameDepthBuffer);
    gl->glUniform1i(getUniform("firstPass_depthTexture"), tex);
    tex++;

    glActiveTexture(GL_TEXTURE0 + tex);
    glBindTexture(GL_TEXTURE_1D, grid->valuesRangeToDisplay);
    gl->glUniform1i(getUniform("valuesRangeToDisplay"), tex);
    tex++;

    glActiveTexture(GL_TEXTURE0 + tex);
    glBindTexture(GL_TEXTURE_1D, grid->valuesRangeColorToDisplay);
    gl->glUniform1i(getUniform("colorRangeToDisplay"), tex);
    tex++;

    float val = 1.;
    if(drawFront)
        val = 0.;
    gl->glUniform1fv(getUniform("isFirstPass"), 1, &val);

    glm::vec3 floatres = glm::convert_to<float>(grid->grid->sampler.getSamplerDimension());
    gl->glUniform3fv(getUniform("voxelSize"), 1, glm::value_ptr(grid->grid->getVoxelSize()));
    gl->glUniform3fv(getUniform("gridSize"), 1, glm::value_ptr(floatres));
    gl->glUniform3fv(getUniform("visuBBMin"), 1, glm::value_ptr(grid->grid->bbMin));
    gl->glUniform3fv(getUniform("visuBBMax"), 1, glm::value_ptr(grid->grid->bbMax));
    gl->glUniform1ui(getUniform("shouldUseBB"), 0);
    gl->glUniform1f(getUniform("maxValue"), grid->maxValue);
    gl->glUniform3fv(getUniform("volumeEpsilon"), 1, glm::value_ptr(grid->defaultEpsilon));

    gl->glUniform3fv(getUniform("cam"), 1, glm::value_ptr(camPos));
    gl->glUniform3fv(getUniform("cut"), 1, glm::value_ptr(planePosition));
    gl->glUniform3fv(getUniform("cutDirection"), 1, glm::value_ptr(planeDirection));
    // Clip distance
    gl->glUniform1f(getUniform("clipDistanceFromCamera"), 5.f);

    gl->glUniform1ui(getUniform("displayWireframe"), this->displayTetmesh);
    gl->glUniform1f(getUniform("blendFirstPass"), this->blendFirstPass);

    int drawOnly = 1;
    if(!this->drawOnlyBoundaries)
        drawOnly = 0;
    gl->glUniform1i(getUniform("drawOnlyBoundaries"), drawOnly);

    gl->glUniformMatrix4fv(getUniform("mMat"), 1, GL_FALSE, glm::value_ptr(glm::mat4(1.f)));
    gl->glUniformMatrix4fv(getUniform("vMat"), 1, GL_FALSE, mvMat);
    gl->glUniformMatrix4fv(getUniform("pMat"), 1, GL_FALSE, pMat);

    gl->glUniform1f(getUniform("specRef"), .8f);
    gl->glUniform1f(getUniform("shininess"), .8f);
    gl->glUniform1f(getUniform("diffuseRef"), .8f);

    gl->glUniform3fv(getUniform("color0"), 1, glm::value_ptr(grid->color_0));
    gl->glUniform3fv(getUniform("color1"), 1, glm::value_ptr(grid->color_1));
    gl->glUniform3fv(getUniform("color0Alternate"), 1, glm::value_ptr(grid->color_0));
    gl->glUniform3fv(getUniform("color1Alternate"), 1, glm::value_ptr(grid->color_1));

    glActiveTexture(GL_TEXTURE0 + tex);
    glBindTexture(GL_TEXTURE_1D, this->tex_colorScale_greyscale);
    gl->glUniform1i(getUniform("colorScales[0]"), tex);
    tex++;

    glActiveTexture(GL_TEXTURE0 + tex);
    glBindTexture(GL_TEXTURE_1D, this->tex_colorScale_hsv2rgb);
    gl->glUniform1i(getUniform("colorScales[1]"), tex);
    tex++;

    glActiveTexture(GL_TEXTURE0 + tex);
    glBindTexture(GL_TEXTURE_1D, this->tex_colorScale_user);
    gl->glUniform1i(getUniform("colorScales[2]"), tex);
    tex++;

    glActiveTexture(GL_TEXTURE0 + tex);
    glBindTexture(GL_TEXTURE_1D, this->tex_colorScale_user);
    gl->glUniform1i(getUniform("colorScales[3]"), tex);
    tex++;

    gl->glBindBufferBase(GL_UNIFORM_BUFFER, 0, grid->uboHandle_colorAttributes);
}

GLuint DrawableGrid::uploadTexture1D(const TextureUpload& tex) {
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
