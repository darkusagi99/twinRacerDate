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
#define BASE_HEIGHT 1200
#define SEGMENT_SIZE 200
#define ROAD_WIDTH 800
#define ROAD_SIZE ROAD_LENGTH*SEGMENT_SIZE

int width = 400;
int height = 240;

int segL = 200;
float camD = 0.84f;

struct Line {
	float x, y, z; // center of line
	float X, Y, W; // Screen coordinates
	float scale; // Size of road part displayed
	float curve; // curve of the road
	float spritePos; // Position of the sprite
	float spriteClip; // Clipping of the sprite
	LCDBitmap* spriteLine; // Sprite of the tree - At line level in order to allo different

};

struct Line road[ROAD_LENGTH];

// Design infos
LCDBitmap* decorBitmap;
LCDBitmap* carBitmap;
LCDBitmap* car2Bitmap;
LCDBitmap* carLeftBitmap;
LCDBitmap* carLeft2Bitmap;
LCDBitmap* carRightBitmap;
LCDBitmap* carRight2Bitmap;

LCDBitmap* carDisplayBitmap;
LCDBitmap* carDisplay2Bitmap;
LCDBitmap* palmBitmap;

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


		// Load graphics
		decorBitmap = loadImageAtPath("image/decor.png", pd);
		carBitmap = loadImageAtPath("image/car.png", pd);
		car2Bitmap = loadImageAtPath("image/car2.png", pd);
		carLeftBitmap = loadImageAtPath("image/carLeft.png", pd);
		carLeft2Bitmap = loadImageAtPath("image/carLeft2.png", pd);
		carRightBitmap = loadImageAtPath("image/carRight.png", pd);
		carRight2Bitmap = loadImageAtPath("image/carRight2.png", pd);
		palmBitmap = loadImageAtPath("image/palm.png", pd);


		carDisplayBitmap = carBitmap;
		carDisplay2Bitmap = car2Bitmap;

		// Init road Data
		for (int i = 0; i < ROAD_LENGTH; i++) {

			road[i].x = 0;
			road[i].y = 0;
			road[i].z = i * segL;
			road[i].scale = 1;

			// Ligne droite par défaut
			road[i].curve = 0;

			// Définition d'un virage
			if (i > 400 && i < 800) { road[i].curve = 1; }

			// Up and down road position
			if (i > 1300) { road[i].y = sin(i / 30.0) * BASE_HEIGHT; }

			// Add sprite data
			road[i].spritePos = 0;
			road[i].spriteLine = NULL;

			// Tree on the
			if (i % 20 == 0) {
				road[i].spritePos = -1.5;
				road[i].spriteLine = palmBitmap;
			}

			road[i].X = 200;
			road[i].Y = 240;

		}


		// Note: If you set an update callback in the kEventInit handler, the system assumes the game is pure C and doesn't run any Lua code in the game
		pd->system->setUpdateCallback(update, pd);
	}
	
	return 0;
}



// Projection world to screen
void projectionLine(struct Line* line , int camX, int camY, int camZ) {

	float zTmp;
	if(line->z < camZ) {
		zTmp = ROAD_SIZE + line->z - camZ;
	}
	else {
		zTmp = line->z - camZ;
	}

	line->scale = fabs(camD / zTmp);
	if (isinf(line->scale)) { line->scale = 1; }
	line->X = (1 + line->scale * (line->x - camX)) * width / 2;
	line->Y = (1 - line->scale * (line->y - camY)) * height / 2;
	line->W = line->scale * ROAD_WIDTH * width / 2;

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

void drawSprite(PlaydateAPI* pd, struct Line* currLine) {


	// Pas d'affichage, on sort
	if (currLine->spriteLine == NULL) { return; }

	// Hardcoded sprite size
	int w = 84;
	int h = 135;

	float destX = currLine->X + currLine->scale * currLine->spritePos * width / 2;
	float destY = currLine->Y + 4;
	float destW = w * currLine->W / 30;
	float destH = h * currLine->W / 30;

	destX += destW * currLine->spritePos;
	destY += destH * (- 1);


	float clipH = destY + destH - currLine->spriteClip;

	if (clipH < 0) { clipH = 0; }

	// Pas d'affichage, on sort
	//if (clipH >= destH) { return; }


	// Draw sprite
	pd->graphics->setDrawMode(kDrawModeNXOR);
	float xscale = destW / w;
	float yscale = destH / h;
	pd->graphics->drawScaledBitmap(currLine->spriteLine, destX, destY, destW / w, destH / h);


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

// Height of camera
int camH = BASE_HEIGHT;


static int update(void* userdata)
{
	PlaydateAPI* pd = userdata;

	// Read input
	pd->system->getButtonState(&currentBtn, &pushedBtn, &releasedBtn);

	// Read A button
	if (pushedBtn & kButtonA) { driving = 1; }
	if (releasedBtn & kButtonA) { driving = 0; }

	// Left / right
	if (currentBtn & kButtonLeft) { 
		posX -= CAR_STRAFE; 

		carDisplayBitmap = carLeftBitmap;
		carDisplay2Bitmap = carLeft2Bitmap;
	}
	if (currentBtn & kButtonRight) { 
		posX += CAR_STRAFE;

		carDisplayBitmap = carRightBitmap;
		carDisplay2Bitmap = carRight2Bitmap;
	}


	if ((releasedBtn & kButtonRight) || (releasedBtn & kButtonLeft)) {
		carDisplayBitmap = carBitmap;
		carDisplay2Bitmap = car2Bitmap;
	}

	// Make the car go forward
	if (driving) { posZ += CAR_SPEED; }
	
	pd->graphics->clear(kColorWhite);

	while (posZ >= ROAD_LENGTH * segL) { posZ = posZ % ROAD_SIZE; }

	int startPos = posZ / segL;
	int prevPos = (ROAD_LENGTH + startPos - 1) % ROAD_LENGTH;
	float x = 0; // Curve element
	float dx = 0; // Delta for curve

	// Draw decor
	pd->graphics->setDrawMode(kDrawModeCopy);
	pd->graphics->drawBitmap(decorBitmap, posX/200, 0, kBitmapUnflipped);

	// Draw road
	prevLine = &road[prevPos];
	projectionLine(prevLine, posX, camH, posZ);


	currLine = &road[startPos];
	projectionLine(currLine, posX - x, camH, posZ);

	// Camera Height
	camH = BASE_HEIGHT + currLine->y;
	int maxY = height;

	//pd->system->logToConsole("RoadLoop : %d, %d, %d", startPos, 200 + startPos, (200 + startPos) % ROAD_LENGTH);
	for (int j = startPos; j < 200 + startPos; j++) {

		// Get line
		currLine = &road[j % ROAD_LENGTH];
		//pd->system->logToConsole("RoadLoopCurrent : %d, %d", j, j % ROAD_LENGTH);

		// Calculate screen coordinates
		projectionLine(currLine, posX - x, camH, posZ);
		x += dx;
		dx += currLine->curve;

		// Ignore "Too high lines"
		maxY = currLine->Y;

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

	for (int j = startPos; j < 200 + startPos; j++) {

		// Get line
		currLine = &road[j % ROAD_LENGTH];

		// Draw sprite (if exists)
		drawSprite(pd, currLine);

		prevLine = currLine;

	}

	// Draw the car
	pd->graphics->setDrawMode(kDrawModeWhiteTransparent);
	pd->graphics->drawBitmap(carDisplayBitmap, 160, 154, kBitmapUnflipped);

	pd->graphics->setDrawMode(kDrawModeBlackTransparent);
	pd->graphics->drawBitmap(carDisplay2Bitmap, 160, 154, kBitmapUnflipped);
        
	// Draw FPS
	pd->system->drawFPS(0,0);

	return 1;
}

