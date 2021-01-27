#version 400 core

in vec4 vPos_WS;
in vec4 vNorm_WS;
in vec3 texCoord;
in vec3 barycentricCoords;
in vec3 largestDelta;

in vec4 vPos_CS;
in vec4 vNorm_CS;
in vec4 lightDir_CS;
in vec4 eyeDir_CS;

out vec4 color;

uniform uint minTexVal;
uniform uint maxTexVal;
uniform vec2 texBounds;		// Min/max values of the texture to show.
uniform vec2 colorBounds;	// Min/max values to compute the color scale
uniform uint colorOrTexture;

// Draw modes :
//    - 0u : texture/cube/polygons only
//    - 1u : 0u, but with wireframe on top
//    - 2u : wireframe only, transparent
uniform uint drawMode;

uniform vec4 lightPos;
uniform usampler3D texData; // texture generated by the voxel grid generation
uniform sampler1D colorScale; // Color scale generated by the program

uniform vec3 sceneBBPosition;
uniform vec3 sceneBBDiagonal;
uniform vec3 voxelGridSize;
uniform vec3 planePositions;
uniform vec3 planeDirections;

uniform uint nbChannels;	// nb of channels in the image (R, RG, RGB ?)

/// Takes a uvec3 of an R8UI-based texture and spits out an RGB color by applying a 'realistic' color grading
vec4 R8UIToRGB(in uvec3 ucolor);
vec4 R8UIToRGB_1channel(in uvec3 ucolor);
vec4 R8UIToRGB_2channel(in uvec3 ucolor);

// Takes a uvec3 of an R8UI-based texture and spits out an RGB color by looking up the color scale
vec4 R8UItoColorScale(in uvec3 ucolor);
// Function alternating between the color scale and HSV2RGB texture output from user input.
vec4 R8UIConversion(in uvec3 ucolor);
// Get a plane's coordinate in its axis.
float planeIdxToPlanePosition(int id);
// Checks a fragment is visible, according to the plane positions and directions.
bool isFragmentVisible();

// 'k' taken from Brian's paper
float color_k = 2.5;

void main(void)
{
	// Compute the cutting plane position so that we can threshold the fragments :
	if (isFragmentVisible() == false) { discard; }

	float epsilon = .03;
	float distMin = min(barycentricCoords.x/largestDelta.x, min(barycentricCoords.y/largestDelta.y, barycentricCoords.z/largestDelta.z));

	vec4 basecolor;
	uvec3 ui = texture(texData, texCoord).rgb; // color, as R(uchar)G(void)B(void)
	// If we're in the area of a primitive where wireframe is NOT shown :
	if (distMin > epsilon) {
		basecolor = R8UIConversion(ui);
		// If we asked for wireframe only, show nothing (alpha=.0)
		if (drawMode == 2) {
			basecolor = vec4(.3, .3, .3, .0);
		}
	}
	// If we're in the area of a primitive where wireframe IS shown :
	else {
		// If asked to do wireframe only, set basecolor to grey only, if wireframe+texture, have a
		// stripe of white surrounding the wireframe (better contrast), if texture only, show it :
		float colorRatio = (drawMode == 2u) ? (.3) : (1. - ((distMin/epsilon < .33 || distMin/epsilon > .66) ? 1. : .0));
		basecolor = (drawMode == 0u) ? R8UIConversion(ui) : vec4(colorRatio, colorRatio, colorRatio, 1.);
	}

	color = basecolor;
	//color = vNorm_WS; // debug

	if (basecolor.a < 0.1) { discard; } // if transparent, discard the fragment to show others behind it
}

vec4 R8UIToRGB(in uvec3 ucolor) {
	if (nbChannels == 1u) { return R8UIToRGB_1channel(ucolor); }
	else { return R8UIToRGB_2channel(ucolor); }
}

vec4 R8UIToRGB_1channel(in uvec3 ucolor) {
	float color_r = float(ucolor.r);
	float color_g = float(ucolor.r);
	// Check if we're in the colorscale :
	color_r = clamp(color_r, colorBounds.x, colorBounds.y);
	color_g = clamp(color_g, colorBounds.x, colorBounds.y);
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
		1.
	);
}

vec4 R8UIToRGB_2channel(in uvec3 ucolor) {
	float color_r = float(ucolor.r);
	float color_g = float(ucolor.g);
	// Check if we're in the colorscale :
	color_r = clamp(float(ucolor.r), colorBounds.x, colorBounds.y);
	color_g = clamp(float(ucolor.g), colorBounds.x, colorBounds.y);
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
		1.
	);
}

vec4 R8UItoColorScale(in uvec3 ucolor) {
	if (ucolor.r < minTexVal) { return vec4(.0, .0, .0, 1.); }
	if (ucolor.r > maxTexVal) { return vec4(1., 1., 1., 1.); }
	// Only the red component contains the data needed for the color scale :
	float index = float(ucolor.r)/255.;
	vec4 color = texture(colorScale, index);
	color.a = 1.f;
	return color;
}

vec4 R8UIConversion(in uvec3 ucolor) {
	if (colorOrTexture > 0) { return R8UIToRGB(ucolor); }
	return R8UIToRGB(ucolor);
}

float planeIdxToPlanePosition(int id) {
	// displacement to apply :
	vec3 diff = planePositions - sceneBBPosition;
	if (id == 1) { return diff.x; }
	if (id == 2) { return diff.y; }
	if (id == 3) { return diff.z; }
	return 0.f;
}

bool isFragmentVisible() {
	float epsilon = .01f;
	if (((vPos_WS.x - planePositions.x) * planeDirections.x + epsilon) < .0f) { return false; }
	if (((vPos_WS.y - planePositions.y) * planeDirections.y + epsilon) < .0f) { return false; }
	if (((vPos_WS.z - planePositions.z) * planeDirections.z + epsilon) < .0f) { return false; }
	return true;
}
