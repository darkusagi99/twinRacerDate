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
#define ROAD_PART_LENGTH 50
#define MAX_SPEED 300
#define ACCEL 5
#define BREAKING 10
#define DECEL 2
#define OFFROAD_DECEL 15
#define CAR_STRAFE 20
#define BASE_HEIGHT 1200
#define SEGMENT_SIZE 200
#define ROAD_WIDTH 1000
#define ROAD_SIZE ROAD_LENGTH*SEGMENT_SIZE
#define SCREEN_WIDTH 400

int width = 400;
int height = 240;

int segL = 200;
float camD = 0.84f;

float speed = 0;
float carX = 0;
int posZ = 0;
int driving = 0;
int timer = 60 * 30; // 30 seconds at 60fps
int score = 0;
int gameOver = 0;

struct Line {
	float x, y, z; // center of line
	float X, Y, W; // Screen coordinates
	float scale; // Size of road part displayed
	float curve; // curve of the road
	float spritePos; // Position of the sprite
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
void generate_road_data(void) {
	// Init road Data
	for (int i = 0; i < ROAD_LENGTH/ROAD_PART_LENGTH; i++) {

		int curveVal = rand() % 2;
		int hasCurve = rand() % 100;

		int hasUpDown = rand() % 100;
		int hasPalm = rand() % 100;

		for (int j = 0; j < ROAD_PART_LENGTH; j++) {
			int idx = (i*ROAD_PART_LENGTH)+j;
			road[idx].x = 0;
			road[idx].y = 0;
			road[idx].z = idx * segL;
			road[idx].scale = 1;

			// Ligne droite par d�faut
			road[idx].curve = 0;
			// 20% change of curve
			if (hasCurve > 60) {
				if (curveVal) {
					road[idx].curve = -1;
				} else {
					road[idx].curve = 1;
				}
			}

			// Up and down road position
			if (hasUpDown > 90) { road[idx].y = sin(idx / 30.0) * BASE_HEIGHT; }

			// Add sprite data
			road[idx].spritePos = 0;
			road[idx].spriteLine = NULL;

			// Tree near the track border
			if ((hasPalm > 30) && (idx % 20 == 0)) {
				road[idx].spritePos = -1.5;
				road[idx].spriteLine = palmBitmap;
			}

			road[idx].X = 200;
			road[idx].Y = 240;
		}

	}
}

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

		generate_road_data();


		// Note: If you set an update callback in the kEventInit handler, the system assumes the game is pure C and doesn't run any Lua code in the game
		pd->system->setUpdateCallback(update, pd);
	}
	
	return 0;
}



// Projection world to screen
void projectionLine(struct Line* line , int camX, int camY, int camZ) {

	float zTmp;
	if(line->z + 1000 < camZ) {
		zTmp = ROAD_SIZE + line->z - camZ;
	}
	else {
		zTmp = line->z - camZ;
	}

	line->scale = fabsf(camD / zTmp);
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
	float destY = currLine->Y;
	float destW = w * currLine->W / 30;
	float destH = h * currLine->W / 30;

	destX += destW * currLine->spritePos;
	destY += destH * (- 1);

	// Draw sprite
	pd->graphics->setDrawMode(kDrawModeWhiteTransparent);
	pd->graphics->drawScaledBitmap(currLine->spriteLine, destX, destY, destW / w, destH / h);


}

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

	if (gameOver) {
		pd->graphics->clear(kColorBlack);
		pd->graphics->setFont(font);
		pd->graphics->setDrawMode(kDrawModeFillWhite);
		char scoreStr[32];
		sprintf(scoreStr, "Score: %d", score);

		pd->graphics->drawTextInRect("GAME OVER", 9, kASCIIEncoding, 0, 80, SCREEN_WIDTH, 40, kWrapWord, kAlignTextCenter);
		pd->graphics->drawTextInRect(scoreStr, strlen(scoreStr), kASCIIEncoding, 0, 110, SCREEN_WIDTH, 40, kWrapWord, kAlignTextCenter);
		pd->graphics->drawTextInRect("Press A to Restart", 18, kASCIIEncoding, 0, 140, SCREEN_WIDTH, 40, kWrapWord, kAlignTextCenter);

		
		pd->system->getButtonState(&currentBtn, &pushedBtn, &releasedBtn);
		if (pushedBtn & kButtonA) {
			gameOver = 0;
			speed = 0;
			posZ = 0;
			carX = 0;
			timer = 60 * 30;
			score = 0;
			generate_road_data();
		}
		return 1;
	}

	// Read input
	pd->system->getButtonState(&currentBtn, &pushedBtn, &releasedBtn);

	// Read A button
	if (currentBtn & kButtonA) { 
		speed += ACCEL; 
	} else {
		speed -= DECEL;
	}

	if (currentBtn & kButtonB) {
		speed -= BREAKING;
	}

	// Offroad slowdown
	if (carX < -ROAD_WIDTH || carX > ROAD_WIDTH) {
		if (speed > 50) speed -= OFFROAD_DECEL;
	}

	if (speed < 0) speed = 0;
	if (speed > MAX_SPEED) speed = MAX_SPEED;

	// Left / right
	if (currentBtn & kButtonLeft) { 
		carX -= CAR_STRAFE + (CAR_STRAFE * (speed / MAX_SPEED));

		carDisplayBitmap = carLeftBitmap;
		carDisplay2Bitmap = carLeft2Bitmap;
	}
	else if (currentBtn & kButtonRight) { 
		carX += CAR_STRAFE + (CAR_STRAFE * (speed / MAX_SPEED));

		carDisplayBitmap = carRightBitmap;
		carDisplay2Bitmap = carRight2Bitmap;
	}
	else {
		carDisplayBitmap = carBitmap;
		carDisplay2Bitmap = car2Bitmap;
	}


	// Make the car go forward
	posZ += speed;

	// Centrifugal force in curves
	int currentSeg = (posZ / segL) % ROAD_LENGTH;
	carX -= speed * road[currentSeg].curve * 0.1f;
	
	// Timer and Score
	timer--;
	if (speed > 0) {
		score += (int)(speed / 10);
	}
	if (timer <= 0) {
		gameOver = 1;
	}

	pd->graphics->clear(kColorWhite);

	while (posZ >= ROAD_LENGTH * segL) { posZ = posZ % ROAD_SIZE; }

	int startPos = posZ / segL;
	int prevPos = (ROAD_LENGTH + startPos - 1) % ROAD_LENGTH;
	float x = 0; // Curve element
	float dx = 0; // Delta for curve

	// Draw decor
	pd->graphics->setDrawMode(kDrawModeCopy);
	pd->graphics->drawBitmap(decorBitmap, (int)carX/20, 0, kBitmapUnflipped);

	// Draw road
	prevLine = &road[prevPos];
	projectionLine(prevLine, (int)carX, camH, posZ);


	currLine = &road[startPos];
	projectionLine(currLine, (int)carX - x, camH, posZ);

	// Camera Height
	camH = BASE_HEIGHT + currLine->y;
	float maxY = height*2;


	// Calculate line coordinates
	for (int j = startPos; j < 200 + startPos; j++) {

		// Get line
		currLine = &road[j % ROAD_LENGTH];

		// Calculate screen coordinates
		projectionLine(currLine, (int)carX - x, camH, posZ);
		x += dx;
		dx += currLine->curve;

		prevLine = currLine;

	}

	// Draw the palm in separate loop (overlap - First pass)
	for (int j = startPos; j < 200 + startPos; j++) {

		// Get line
		currLine = &road[j % ROAD_LENGTH];

		// Draw sprite (if exists)
		drawSprite(pd, currLine);

		prevLine = currLine;

	}

	// Reset loop vars
	prevLine = &road[prevPos];
	maxY = height * 2;
	// Draw road
	for (int j = startPos; j < 200 + startPos; j++) {

		// Get line
		currLine = &road[j % ROAD_LENGTH];

		// Ignore "Too high lines"
		if (currLine->Y >= maxY) { continue; }
		maxY = currLine->Y;

		// Choose line color
		if ((j / 3) % 2) {
			grassColor = (LCDColor) grassLight;
			rumbleColor = rumbleLight;
			roadColor = (LCDColor) roadLight;
		}
		else {
			grassColor = (LCDColor) grassDark;
			rumbleColor = (LCDColor) rumbleDark;
			roadColor = (LCDColor) roadDark;
		}

		// Draw road part
		// Grass element
		drawWedge(pd, grassColor, 0, prevLine->Y, width, 0, currLine->Y, width);

		// Rumble element
		drawWedge(pd, rumbleColor, prevLine->X, prevLine->Y, (int)(prevLine->W * 1.2f), currLine->X, currLine->Y, (int)(currLine->W * 1.2f));

		// Road Element
		drawWedge(pd, roadColor, prevLine->X, prevLine->Y, (int)prevLine->W, currLine->X, currLine->Y, (int)currLine->W);

		prevLine = currLine;

	}


	// Draw the palm in separate loop (overlap - only ones in "front")
	maxY = height * 2;
	for (int j = startPos; j < 200 + startPos; j++) {

		// Get line
		currLine = &road[j % ROAD_LENGTH];

		// Ignore "Too high lines"
		if (currLine->Y >= maxY) { continue; }
		maxY = currLine->Y;

		// Draw sprite (if exists)
		drawSprite(pd, currLine);

		prevLine = currLine;

	}

	// Draw the car
	pd->graphics->setDrawMode(kDrawModeWhiteTransparent);
	pd->graphics->drawBitmap(carDisplayBitmap, 160, 154, kBitmapUnflipped);

	pd->graphics->setDrawMode(kDrawModeBlackTransparent);
	pd->graphics->drawBitmap(carDisplay2Bitmap, 160, 154, kBitmapUnflipped);
        
	// HUD
	pd->graphics->setDrawMode(kDrawModeNXOR);
	pd->graphics->setFont(font);
	char hudStr[64];
	sprintf(hudStr, "Speed: %d km/h", (int)(speed * 0.8f));
	pd->graphics->drawText(hudStr, strlen(hudStr), kASCIIEncoding, 10, 210);
	
	sprintf(hudStr, "Time: %d", timer / 60);
	pd->graphics->drawText(hudStr, strlen(hudStr), kASCIIEncoding, 320, 210);

	sprintf(hudStr, "Score: %d", score);
	pd->graphics->drawText(hudStr, strlen(hudStr), kASCIIEncoding, 10, 10);


	return 1;
}

