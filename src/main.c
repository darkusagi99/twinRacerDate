//
//  main.c
//  Extension
//
//  Created by Dave Hayden on 7/30/14.
//  Copyright (c) 2014 Panic, Inc. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>

#include "pd_api.h"

static int update(void* userdata);
const char* fontpath = "/System/Fonts/Asheville-Sans-14-Bold.pft";
LCDFont* font = NULL;

#define TEXT_WIDTH 86
#define TEXT_HEIGHT 16

#define POLY_PTS 4
#define ROAD_LENGTH 1600

int x = (400 - TEXT_WIDTH) / 2;
int y = (240 - TEXT_HEIGHT) / 2;
int dx = 1;
int dy = 2;

int width = 400;
int height = 240;

int roadW = 800;
int segL = 200;
float camD = 0.84;

struct Line {
	float x, y, z; // center of line
	float X, Y, W; // Screen coordinates
	float scale;
};

struct Line road[ROAD_LENGTH];

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

		}

		// Note: If you set an update callback in the kEventInit handler, the system assumes the game is pure C and doesn't run any Lua code in the game
		pd->system->setUpdateCallback(update, pd);
	}
	
	return 0;
}



// Projection world to screen
void projectionLine(struct Line* line , int camX, int camY, int camZ) {
	line->scale = camD / (line->z - camZ);
	line->X = (1 + line->scale * (line->x - camX)) * width / 2;
	line->Y = (1 - line->scale * (line->y - camY)) * height / 2;
	line->W = line->scale * roadW * width / 2;

}


// Global var for coordinates
int coords[8];

LCDColor grassLight = kColorWhite;
LCDColor grassDark = kColorWhite;
LCDColor rumbleLight = kColorBlack;
LCDColor rumbleDark = kColorBlack;
LCDColor roadLight = kColorBlack;
LCDColor roadDark = kColorBlack;


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

char debugLine[200];

static int update(void* userdata)
{
	PlaydateAPI* pd = userdata;
	
	pd->graphics->clear(kColorWhite);

	// Draw road
	for (int i = 0; i < 30; i++) {
	
		// Get line
		struct Line currLine = road[i % ROAD_LENGTH];
		struct Line prevLine = road[(i-1) % ROAD_LENGTH];

		// Calculate screen coordinates
		projectionLine(&prevLine, 0, 1200, 0);
		projectionLine(&currLine, 0, 1200, 0);

		// Choose line color
		if ((i / 3) % 2) {
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
		drawWedge(pd, grassColor, 0, prevLine.Y, width, 0, currLine.Y, width);

		// Rumble element
		drawWedge(pd, rumbleColor, prevLine.X, prevLine.Y, prevLine.W * 1.2, currLine.X, currLine.Y, currLine.W * 1.2);

		// Road Element
		drawWedge(pd, kColorBlack, prevLine.X, prevLine.Y, prevLine.W, currLine.X, currLine.Y, currLine.W);


	}

        
	pd->system->drawFPS(0,0);

	return 1;
}

