#pragma once

#include <string>

struct GraphicsDefinition
{
	std::string filename;
	int x;
	int y;
	int width;
	int height;

	bool flip_x;
	bool flip_y;
	
	float shiftx;
	float shifty;

	float scale;

	// how to draw a thing:
	// 1. load the image denoted by filename
	// 2. crop it to the rectangle with topleft=(x,y) and size=(width,height)
	// 3. scale it by the factor "scale * desired_pixels_per_tile / 32"
	// 4. draw it, so that its center sits at tile position (entity.x,entity.y) + (shiftx, shifty)
};
