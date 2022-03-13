//
//  main.c
//  Extension
//
//  Created by Dave Hayden on 7/30/14.
//  Copyright (c) 2014 Panic, Inc. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "pd_api.h"

static int update(void* userdata);
const char* fontpath = "/System/Fonts/Asheville-Sans-14-Bold.pft";
LCDFont* font = NULL;

#define POLY_PTS 4
#define ROAD_LENGTH 1600
#define CAR_SPEED 250
#define CAR_STRAFE 20

int width = 400;
int height = 240;

int roadW = 800;
int segL = 200;
float camD = 0.84f;

struct Line {
	float x, y, z; // center of line
	float X, Y, W; // Screen coordinates
	float scale; // Size of road part displayed
	float curve; // curve of the road
};

struct Line road[ROAD_LENGTH];

// Design infos
LCDBitmap* decorBitmap;

// Bitmap loader
LCDBitmap* loadImageAtPath(const char* path, PlaydateAPI* pd)
{
	const char* outErr = NULL;
	LCDBitmap* img = pd->graphics->loadBitmap(path, &outErr);
	if (outErr != NULL) {
		pd->system->logToConsole("Error loading image at path '%s': %s", path, outErr);
	}
	return img;
}

#ifdef _WINDLL
__declspec(dllexport)
#endif
int eventHandler(PlaydateAPI* pd, PDSystemEvent event, uint32_t arg)
{
	(void)arg; // arg is currently only used for event = kEventKeyPressed

	if ( event == kEventInit )
	{
		const char* err;
		font = pd->graphics->loadFont(fontpath, &err);
		
		if ( font == NULL )
			pd->system->error("%s:%i Couldn't load font %s: %s", __FILE__, __LINE__, fontpath, err);

		// Init road Data
		for (int i = 0; i < ROAD_LENGTH; i++) {

			road[i].x = 0;
			road[i].y = 0;
			road[i].z = i * segL;
			road[i].scale = 1;

			road[i].X = 200;
			road[i].Y = 240;

		}

		// Load graphics
		decorBitmap = loadImageAtPath("image/decor.png", pd);

		// Note: If you set an update callback in the kEventInit handler, the system assumes the game is pure C and doesn't run any Lua code in the game
		pd->system->setUpdateCallback(update, pd);
	}
	
	return 0;
}



// Projection world to screen
void projectionLine(struct Line* line , int camX, int camY, int camZ) {

	line->scale = fabs(camD / (line->z - camZ));
	if (isinf(line->scale)) { line->scale = 1; }
	line->X = (1 + line->scale * (line->x - camX)) * width / 2;
	line->Y = (1 - line->scale * (line->y - camY)) * height / 2;
	line->W = line->scale * roadW * width / 2;

}


// Global var for coordinates
int coords[8];

// "Colors" definitions
LCDPattern grassLight = {255, 255, 255, 255, 222, 255, 255, 255, 
						255, 255, 255, 255, 255, 255, 255, 255};
LCDPattern grassDark = { 254, 255, 222, 255, 255, 255, 255, 255, 
						255, 255, 255, 255, 255, 255, 255, 255 };
LCDColor rumbleLight = kColorWhite;
LCDPattern rumbleDark = { 40, 18, 37, 64, 128, 12, 64, 32,
						255, 255, 255, 255, 255, 255, 255, 255 };
LCDPattern roadLight = { 16, 0, 128, 0, 0, 64, 0, 32,
						255, 255, 255, 255, 255, 255, 255, 255 };
LCDPattern roadDark = { 0, 0, 0, 0, 0, 0, 0, 0,
						255, 255, 255, 255, 255, 255, 255, 255 };


LCDColor grassColor;
LCDColor rumbleColor;
LCDColor roadColor;

void drawWedge(PlaydateAPI* pd, LCDColor color, int x1, int y1, int w1, int x2, int y2, int w2) {

	coords[0] = x1 - w1;
	coords[1] = y1;
	coords[2] = x1 + w1;
	coords[3] = y1;
	coords[4] = x2 + w2;
	coords[5] = y2;
	coords[6] = x2 - w2;
	coords[7] = y2;

	pd->graphics->fillPolygon(POLY_PTS, coords, color, kPolygonFillNonZero);

}

// Car Z position
int posZ = 0;
int posX = 0;
int driving = 0;

// Button states
PDButtons pushedBtn;
PDButtons releasedBtn;
PDButtons currentBtn;

// Line infos
struct Line* currLine;
struct Line* prevLine;



static int update(void* userdata)
{
	PlaydateAPI* pd = userdata;

	// Read input
	pd->system->getButtonState(&currentBtn, &pushedBtn, &releasedBtn);

	// Read A button
	if (pushedBtn & kButtonA) { driving = 1; }
	if (releasedBtn & kButtonA) { driving = 0; }

	// Left / right
	if (currentBtn & kButtonLeft) { posX -= CAR_STRAFE; }
	if (currentBtn & kButtonRight) { posX += CAR_STRAFE; }

	// Make the car go forward
	if (driving) { posZ += CAR_SPEED; }
	
	pd->graphics->clear(kColorWhite);

	int startPos = posZ / segL;
	int prevPos = (ROAD_LENGTH + startPos - 1) % ROAD_LENGTH;

	// Draw decor
	pd->graphics->drawBitmap(decorBitmap, posX/200, 0, kBitmapUnflipped);

	// Draw road
	prevLine = &road[prevPos];
	projectionLine(prevLine, posX, 1200, posZ);

	for (int j = startPos; j < 87 + startPos; j++) {

		// Get line
		currLine = &road[j % ROAD_LENGTH];

		// Calculate screen coordinates
		projectionLine(currLine, posX, 1200, posZ);

		// Choose line color
		if ((j / 3) % 2) {
			grassColor = grassLight;
			rumbleColor = rumbleLight;
			roadColor = roadLight;
		}
		else {
			grassColor = grassDark;
			rumbleColor = rumbleDark;
			roadColor = roadDark;
		}

		// Draw road part
		// Grass element
		drawWedge(pd, grassColor, 0, prevLine->Y, width, 0, currLine->Y, width);

		// Rumble element
		drawWedge(pd, rumbleColor, prevLine->X, prevLine->Y, prevLine->W * 1.2, currLine->X, currLine->Y, currLine->W * 1.2);

		// Road Element
		drawWedge(pd, roadColor, prevLine->X, prevLine->Y, prevLine->W, currLine->X, currLine->Y, currLine->W);

		prevLine = currLine;

	}

        
	pd->system->drawFPS(0,0);

	return 1;
}

