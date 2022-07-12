#version 150 core

// Signals we're in the main shader, for any shaders inserted into this one.
#define MAIN_SHADER_UNIT

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_explicit_attrib_location : enable

/****************************************/
/**************** Inputs ****************/
/****************************************/
layout(location = 0) in vec4 vPos;		// The vertex's positions
layout(location = 1) in vec3 vOriginalCoords;	// The vertex's normal
layout(location = 2) in vec3 vTexCoords;	// The vertex's texture coordinates
layout(location = 3) in vec2 planeMultiplier;	// The multiplier used to 'stretch' the plane

/****************************************/
/*************** Outputs ****************/
/****************************************/
layout(location = 0) out vec4 color; // This fragment's color
layout(location = 1) out vec4 finalGridCoordinates;	// The normalized scene coordinates used to fetch textures

/****************************************/
/*************** Uniforms ***************/
/****************************************/
uniform usampler3D texData;	// texture generated by the voxel grid generation
uniform uint planeIndex;	// The identifier of the currently drawn plane.
uniform vec4 sceneBBDiagonal;	// The grid's world-space bounding box diagonal
uniform vec4 sceneBBPosition;	// The grid's world-space bounding box position

// The structure which defines every attributes for the color channels.
struct colorChannelAttributes {
	uint isVisible;			// /*align : ui64*/ Is this color channel enabled/visible ?
	uint colorScaleIndex;	// /*align : ui64*/ The color channel to choose
	uvec2 visibleBounds;	// /*align : vec4*/ The bounds of the visible values
	uvec2 colorScaleBounds;	// /*align : vec4*/ The value bounds for the color scale
};

uniform uint mainChannelIndex;						// The index of the main channel in the voxel data
uniform sampler1D valuesRangeToDisplay;
uniform sampler1D colorRangeToDisplay;
uniform float maxValue;
uniform sampler1D colorScales[4];					// All the color scales available (all encoded as 1D textures)
layout(std140) uniform ColorBlock {
	colorChannelAttributes attributes[4];	// Color attributes laid out in this way : [ main, R, G, B ]
} colorChannels;

// Manipulator display
uniform uint displayManipulator;
uniform float sphereRadius;
uniform uint nbManipulator;
uniform sampler1D manipulatorPositions;	
uniform sampler1D states;	

/****************************************/
/*********** Function headers ***********/
/****************************************/
vec4 planeIdxToColor(in uint idx);
bool shouldDiscard();
bool shouldDrawBorder();
bool checkAndColorizeVoxel(in uvec3 voxel, out vec4 color);
// New function to colorize voxels :
vec4 fragmentEvaluationSingleChannel(in uvec3 color);
vec4 stateToColor(in float current_state);

#pragma include_color_shader;

#line 2056

/****************************************/
/***************** Main *****************/
/****************************************/
void main() {
	if (shouldDiscard()) { if (!shouldDrawBorder()) { finalGridCoordinates = vec4(.0,.0,.0,.0); discard; } }

	// Default color : plane color
	color = planeIdxToColor(planeIndex);

	// If in the border area, stop there :
	if (shouldDrawBorder() == false) {
		// Get the texture data :
		if (vTexCoords.x < .0f || vTexCoords.y < .0f || vTexCoords.z < .0f) { discard; }
		uvec3 tex = texture(texData, vTexCoords).rgb;

		vec4 final_color = fragmentEvaluationSingleChannel(tex);
		if (final_color.w > 0.005f) {
			color = final_color;
		} else {
			// Otherwise, still compute the value but set it with a low alpha
			// to have a bit of context around the texture data.
			color = final_color;
			color.a = .1;
		}
	}

    // Manipulator display
    if(int(displayManipulator) == 1) {
        for(int i = 0; i < int(nbManipulator); ++i) {
            vec3 manipulatorPos = texelFetch(manipulatorPositions, i, 0).xyz;
            float current_state = texelFetch(states, i, 0).x;
            if(distance(vPos.xyz, manipulatorPos) < sphereRadius)
                color = stateToColor(current_state);
        }
    }

	// Alpha is set to a 2.f to signify we're in the texture (the rest will be set to
	// the value specified udring the call to glClearColor, which is usually in [0, 1]).
	finalGridCoordinates = sceneBBDiagonal * vec4(vTexCoords, 1.f); + sceneBBPosition;
}

/****************************************/
/************** Functions ***************/
/****************************************/
vec4 planeIdxToColor(in uint idx) {
	if (idx == 1u) { return vec4(1., .0, .0, 1.); }
	if (idx == 2u) { return vec4(.0, 1., .0, 1.); }
	if (idx == 3u) { return vec4(.0, .0, 1., 1.); }
	return vec4(.27, .27, .27, 1.);
}

vec4 stateToColor(in float current_state) {
    bool isNONE     = abs(current_state - 0) < 0.001;
    bool isAT_RANGE = abs(current_state - 1) < 0.001;
    bool isSELECTED = abs(current_state - 2) < 0.001;
    bool isLOCK     = abs(current_state - 3) < 0.001;
    bool isMOVE     = abs(current_state - 4) < 0.001;
    bool isWAITING   = abs(current_state - 5) < 0.001;
    bool isHIGHLIGHT = abs(current_state - 6) < 0.001;

    vec4 manipulatorColor = vec4(0., 0., 0., 1.);

    if(isNONE) {
        manipulatorColor = vec4(1., 0., 0., 1.);
    }

    if(isAT_RANGE) {
        manipulatorColor = vec4(0., 0.9, 0., 1.);
    }

    if(isMOVE) {
        manipulatorColor = vec4(0., 1., 0., 1.);
    }

    if(isLOCK) {
        manipulatorColor = vec4(0.9, 0.9, 0.9, 1.);
    }

    if(isWAITING) {
        manipulatorColor = vec4(218./255., 112./255., 214./255., 1.);
    }

    if(isHIGHLIGHT) {
        manipulatorColor = vec4(255./255., 215./255., 0./255., 1.);
    }

    return manipulatorColor;
}

bool shouldDiscard() {
	if (vTexCoords.x > 1. || vTexCoords.y > 1. || vTexCoords.z > 1.) { return true; }
	if (vTexCoords.x < 0. || vTexCoords.y < 0. || vTexCoords.z < 0.) { return true; }
	return false;
}

bool shouldDrawBorder() {
	// Create a border around the image (and try to scale border
	// 'lengths' so they're equal on all sides) :
	float min =-.99;
	float max = .99;
	float minx = min;
	float maxx = max;
	float miny = min;
	float maxy = max;
	if ((vOriginalCoords.x) > maxx || (vOriginalCoords.y) > maxy) { return true; }
	if ((vOriginalCoords.x) < minx || (vOriginalCoords.y) < miny) { return true; }
	return false;
}

bool checkAndColorizeVoxel(in uvec3 voxel, out vec4 return_color) {
	return true;
}
