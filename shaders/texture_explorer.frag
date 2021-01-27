#version 400 core

#extension GL_ARB_separate_shader_objects : enable

/****************************************/
/**************** Inputs ****************/
/****************************************/
layout(location = 0) in vec4 vPos;		// The vertex's positions
layout(location = 1) in vec3 vOriginalCoords;	// The vertex's normal
layout(location = 2) in vec3 vTexCoords;	// The vertex's texture coordinates

/****************************************/
/*************** Outputs ****************/
/****************************************/
out vec4 color; // This fragment's color

/****************************************/
/*************** Uniforms ***************/
/****************************************/
uniform usampler3D texData;	// texture generated by the voxel grid generation
uniform sampler1D colorScale;	// Color scale generated by the program
uniform uint planeIndex;	// The identifier of the currently drawn plane.
uniform vec2 colorBounds;	// The min and max values displayed (minTexValue and maxTexValue in other shaders)
uniform vec2 textureBounds;	// The min and max values
uniform uint nbChannels;	// nb of channels in the image (R, RG, RGB)

/****************************************/
/*********** Function headers ***********/
/****************************************/
vec4 voxelValueToColor(in uvec4 ucolor);
vec4 voxelValueToColor_1channel(in uvec4 ucolor);
vec4 voxelValueToColor_2channel(in uvec4 ucolor);
vec4 planeIdxToColor(in uint idx);
bool shouldDiscard();
bool shouldDrawBorder();

/****************************************/
/***************** Main *****************/
/****************************************/
void main() {
	if (shouldDiscard()) { if (!shouldDrawBorder()) { discard; } }

	// Default color : plane color
	color = planeIdxToColor(planeIndex);

	// If in the border area, stop there :
	if (shouldDrawBorder()) { return; }

	color.xyz = vTexCoords;
	color.a = 1.f;
	// Apply the texture :
	color = voxelValueToColor(texture(texData, vTexCoords));
}

/****************************************/
/************** Functions ***************/
/****************************************/
vec4 voxelValueToColor(in uvec4 ucolor) {
	if (nbChannels == 1u) { return voxelValueToColor_1channel(ucolor); }
	else { return voxelValueToColor_2channel(ucolor); }
}

vec4 voxelValueToColor_1channel(in uvec4 ucolor) {
	float alpha = 1.f;
	float r = float(ucolor.r);
	float g = float(ucolor.r);
	if (r < textureBounds.x || r > textureBounds.y) { alpha = .1f; }
	if (g < textureBounds.x || g > textureBounds.y) { alpha = .1f; }
	// Have the R and G color channels clamped to the min/max of the scale
	// (mimics under or over-exposure)
	float color_r = clamp(r, colorBounds.x, colorBounds.y);
	float color_g = clamp(g, colorBounds.x, colorBounds.y);
	// Compute the color as Brian's paper describes it :
	float color_k = 2.5;
	float sc = colorBounds.y - colorBounds.x;
	float eosin = (color_r - colorBounds.x)/(sc);
	float dna = (color_g - colorBounds.x)/(sc); // B is on G channel because OpenGL only allows 2 channels upload to be RG, not RB

	float eosin_r_coef = 0.050;
	float eosin_g_coef = 1.000;
	float eosin_b_coef = 0.544;

	float hematoxylin_r_coef = 0.860;
	float hematoxylin_g_coef = 1.000;
	float hematoxylin_b_coef = 0.300;

	float r_coef = eosin_r_coef;
	float g_coef = eosin_g_coef;
	float b_coef = eosin_b_coef;

	return vec4(
		exp(-hematoxylin_r_coef * dna * color_k) * exp(-eosin_r_coef * eosin * color_k),
		exp(-hematoxylin_g_coef * dna * color_k) * exp(-eosin_g_coef * eosin * color_k),
		exp(-hematoxylin_b_coef * dna * color_k) * exp(-eosin_b_coef * eosin * color_k),
		alpha
	);
}

vec4 voxelValueToColor_2channel(in uvec4 ucolor) {
	float alpha = 1.f;
	float r = float(ucolor.r);
	float g = float(ucolor.g);
	if (r < textureBounds.x || r > textureBounds.y) { alpha = .1f; }
	if (g < textureBounds.x || g > textureBounds.y) { alpha = .1f; }
	// Have the R and G color channels clamped to the min/max of the scale
	// (mimics under or over-exposure)
	float color_r = clamp(r, colorBounds.x, colorBounds.y);
	float color_g = clamp(g, colorBounds.x, colorBounds.y);
	// Compute the color as Brian's paper describes it :
	float color_k = 2.5;
	float sc = colorBounds.y - colorBounds.x;
	float eosin = (color_r - colorBounds.x)/(sc);
	float dna = (color_g - colorBounds.x)/(sc); // B is on G channel because OpenGL only allows 2 channels upload to be RG, not RB

	float eosin_r_coef = 0.050;
	float eosin_g_coef = 1.000;
	float eosin_b_coef = 0.544;

	float hematoxylin_r_coef = 0.860;
	float hematoxylin_g_coef = 1.000;
	float hematoxylin_b_coef = 0.300;

	float r_coef = eosin_r_coef;
	float g_coef = eosin_g_coef;
	float b_coef = eosin_b_coef;

	return vec4(
		exp(-hematoxylin_r_coef * dna * color_k) * exp(-eosin_r_coef * eosin * color_k),
		exp(-hematoxylin_g_coef * dna * color_k) * exp(-eosin_g_coef * eosin * color_k),
		exp(-hematoxylin_b_coef * dna * color_k) * exp(-eosin_b_coef * eosin * color_k),
		alpha
	);
}

vec4 planeIdxToColor(in uint idx) {
	if (idx == 1) { return vec4(1., .0, .0, 1.); }
	if (idx == 2) { return vec4(.0, 1., .0, 1.); }
	if (idx == 3) { return vec4(.0, .0, 1., 1.); }
	return vec4(.27, .27, .27, 1.);
}

bool shouldDiscard() {
	if (vTexCoords.x > 1. || vTexCoords.y > 1. || vTexCoords.z > 1.) { return true; }
	if (vTexCoords.x < 0. || vTexCoords.y < 0. || vTexCoords.z < 0.) { return true; }
	return false;
}

bool shouldDrawBorder() {
	// Create a border around the image :
	float min =-.99;
	float max = .99;
	if (vOriginalCoords.x > max || vOriginalCoords.y > max || vOriginalCoords.z > max) { return true; }
	if (vOriginalCoords.x < min || vOriginalCoords.y < min || vOriginalCoords.z < min) { return true; }
	return false;
}
