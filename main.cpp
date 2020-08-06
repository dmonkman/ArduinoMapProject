// Paul LeClair, 145----
// Drayton Monkman, 155----
// November 24, 2017

/*
	This solution to assignment 2 part 2 is branched from the provided solution for part 1.
	Only this file and restaurant.cpp have been modified form part 1. restaurant.cpp saw the addition of quick sort.
	Some code for touchscreen copied from lecture 13 notes. Other code may be copied from various lecture notes.
*/

#include <Arduino.h>
#include <Adafruit_ILI9341.h>
#include <SPI.h>
#include <SD.h>
#include <TouchScreen.h>

#include "lcd_image.h"
#include "yegmap.h"
#include "restaurant.h"

// TFT display and SD card will share the hardware SPI interface.
// For the Adafruit shield, these are the default.
// The display uses hardware SPI, plus #9 & #10
// Mega2560 Wiring: connect TFT_CLK to 52, TFT_MISO to 50, and TFT_MOSI to 51.
#define TFT_DC 9
#define TFT_CS 10
#define SD_CS 6

// joystick pins
#define JOY_VERT_ANALOG A1
#define JOY_HORIZ_ANALOG A0
#define JOY_SEL 2

// touch screen pins, obtained from the documentaion
#define YP A2  // must be an analog pin, use "An" notation!
#define XM A3  // must be an analog pin, use "An" notation!
#define YM  5  // can be a digital pin
#define XP  4  // can be a digital pin

// width/height of the display when rotated horizontally
#define TFT_WIDTH 320
#define TFT_HEIGHT 240

// layout of display for this assignment,
#define RATING_SIZE 48
#define DISP_WIDTH (TFT_WIDTH - RATING_SIZE)
#define DISP_HEIGHT TFT_HEIGHT

// constants for the joystick
#define JOY_DEADZONE 64
#define JOY_CENTRE 512
#define JOY_STEPS_PER_PIXEL 64

// Cursor size. For best results, use an odd number.
#define CURSOR_SIZE 9

// number of restaurants to display
#define REST_DISP_NUM 30

// gold color used for ratings
#define GOLD 0xFFD0

// calibration data for the touch screen, obtained from documentation
// the minimum/maximum possible readings from the touch point
#define TS_MINX 150
#define TS_MINY 120
#define TS_MAXX 920
#define TS_MAXY 940

// thresholds to determine if there was a touch
#define MINPRESSURE   10
#define MAXPRESSURE 1000

// Use hardware SPI (on Mega2560, #52, #51, and #50) and the above for CS/DC
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

// the currently selected restaurant, if we are in mode 1
int selectedRest = 1;

// store indeces of all restaurants from the current page in an array
int restPage[REST_DISP_NUM];

// store the page number during restaurant selection
int pageNumber = 0;

// how many restaurants are being displayed (matters for the final page)
int restsDisplayed;

// which mode are we in?
int mode;

// which minumum rating is selected?  Default any.
int selectedRating = 3;

// remember which stars are already draw so we don't need to redraw them
bool starDrawn[5] = {0, 0, 0, 0, 0};

// a multimeter reading says there are 300 ohms of resistance across the plate,
// so initialize with this to get more accurate readings
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

// the current map view and the previous one from last cursor movement
MapView curView, preView;

// For sorting and displaying the restaurants, will hold the restaurant RestDist
// information for the most recent click in sorted order.
RestDist restaurants[NUM_RESTAURANTS];

Sd2Card card;

lcd_image_t edmontonBig = { "yeg-big.lcd", MAPWIDTH, MAPHEIGHT };

// The cache of 8 restaurants for the getRestaurant function.
RestCache cache;

// Forward declaration of functions to begin the modes. Setup uses one, so
// it seems natural to forward declare both (not really that important).
void beginMode0();
void beginMode1();

// draw a golden star form the topright corner x, y
void tft_fillStar(int x, int y)
{
	tft.fillRect(x + RATING_SIZE / 4, y + RATING_SIZE / 4, RATING_SIZE / 2, RATING_SIZE / 2, GOLD);
	tft.fillCircle(x + RATING_SIZE / 4, y + RATING_SIZE / 4, RATING_SIZE / 4, ILI9341_BLACK);
	tft.fillCircle(x + 3 * RATING_SIZE / 4, y + RATING_SIZE / 4, RATING_SIZE / 4, ILI9341_BLACK);
	tft.fillCircle(x + RATING_SIZE / 4, y + 3 * RATING_SIZE / 4, RATING_SIZE / 4, ILI9341_BLACK);
	tft.fillCircle(x + 3 * RATING_SIZE / 4, y + 3 * RATING_SIZE / 4, RATING_SIZE / 4, ILI9341_BLACK);
}

void setup() {
	init();

	pinMode(JOY_SEL, INPUT_PULLUP);

	Serial.begin(9600);

	tft.begin();

  Serial.print("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("failed!");
    Serial.println("Is the card inserted properly?");
    while (true) {}
  }

  Serial.print("Initializing SPI communication for raw reads...");
  if (!card.init(SPI_HALF_SPEED, SD_CS)) {
    Serial.println("failed!");
    while (true) {}
  }

  Serial.println("OK!");

	tft.setRotation(-1);
	tft.setTextWrap(false);

  // initial cursor position is the centre of the screen
  curView.cursorX = DISP_WIDTH/2;
  curView.cursorY = DISP_HEIGHT/2;

  // initial map position is the middle of Edmonton
  curView.mapX = ((MAPWIDTH / DISP_WIDTH)/2) * DISP_WIDTH;
  curView.mapY = ((MAPHEIGHT / DISP_HEIGHT)/2) * DISP_HEIGHT;

	// Ensures the first getRestaurant() will load the block as all blocks
	// will start at REST_START_BLOCK, which is 4000000.
	cache.cachedBlock = 0;

  beginMode0();
}

// Draw the rating selector on the side of the screen
void drawRatingSelector() {
	for(int i = 1; i <= 5; i++)
	{
		// Draw a number of stars equal to the rating
		if (i > 5 - selectedRating)
		{
			// Only draw if the star isn't already drawn
			if(starDrawn[i-1] == false)
			{
				tft_fillStar(TFT_WIDTH - RATING_SIZE, (i-1) * RATING_SIZE);
				starDrawn[i-1] = true;
			}
		}
		else
		{
			// Only fill with black if a star is drawn
			if(starDrawn[i-1] == true)
			{
				tft.fillRect(TFT_WIDTH - RATING_SIZE, (i-1) * RATING_SIZE, RATING_SIZE, RATING_SIZE, ILI9341_BLACK);
				starDrawn[i-1] = false;
			}
		}
	}
}

// handle all rating selector functionality
void handleRatingSelector() {
	TSPoint touch = ts.getPoint();

	if (touch.z < MINPRESSURE || touch.z > MAXPRESSURE) {
		// no touch, just quit
		return;
	}

	// get the y coordinate of where the display was touched
	// remember the x-coordinate of touch is really our y-coordinate
	// on the display
	int touchY = map(touch.x, TS_MINX, TS_MAXX, 0, TFT_HEIGHT - 1);

	// need to invert the x-axis, so reverse the
	// range of the display coordinates
	int touchX = map(touch.y, TS_MINY, TS_MAXY, TFT_WIDTH - 1, 0);

	// if a touch occurs in the rating selector area, find where it happened
	if(touchX > DISP_WIDTH && touchX < TFT_WIDTH)
	{
		for(int i = 1; i <= 5; i++)
		{
			if(touchY < RATING_SIZE*i)
			{
				selectedRating = 6 - i;
				drawRatingSelector();
				delay(50);
				return;
			}
		}
	}
}

// Draw the map patch of edmonton over the preView position, then
// draw the red cursor at the curView position.
void moveCursor() {
	lcd_image_draw(&edmontonBig, &tft,
								 preView.mapX + preView.cursorX - CURSOR_SIZE/2,
							 	 preView.mapY + preView.cursorY - CURSOR_SIZE/2,
							   preView.cursorX - CURSOR_SIZE/2, preView.cursorY - CURSOR_SIZE/2,
								 CURSOR_SIZE, CURSOR_SIZE);

	tft.fillRect(curView.cursorX - CURSOR_SIZE/2, curView.cursorY - CURSOR_SIZE/2,
							 CURSOR_SIZE, CURSOR_SIZE, ILI9341_RED);
}

// Set the mode to 0 and draw the map and cursor according to curView
void beginMode0() {
	// Black out the rating selector part (less relevant in Assignment 1, but
	// it is useful when you first start the program).
	tft.fillRect(DISP_WIDTH, 0, RATING_SIZE, DISP_HEIGHT, ILI9341_BLACK);

	// Draw the current part of Edmonton to the tft display.
  lcd_image_draw(&edmontonBig, &tft,
								 curView.mapX, curView.mapY,
								 0, 0,
								 DISP_WIDTH, DISP_HEIGHT);

	// Set all starDrawn variables to false
	for(int i = 0; i < 5; i++)
	{
		starDrawn[i] = 0;
	}

	// initial draw the rating selector
	drawRatingSelector();

	// just the initial draw of the cursor on the map
	moveCursor();

  mode = 0;
}

// Print the i'th restaurant in the sorted list.
// Assumes 0 <= i < 30 for part 1.
void printRestaurant(int i) {
	restaurant r;

	// get the i'th restaurant
	getRestaurant(&r, restaurants[restPage[i]].index, &card, &cache);

	// Set its colour based on whether or not it is the selected restaurant.
	if (i != selectedRest) {
		tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
	}
	else {
		tft.setTextColor(ILI9341_BLACK, ILI9341_WHITE);
	}
	tft.setCursor(0, i*8);
	tft.print(r.name);
}

// overload the printRestaurant function so we don't need to recall the SD from beginMode1()
void printRestaurant(restaurant &r, int i) {
	// Set its colour based on whether or not it is the selected restaurant.
	if (i != selectedRest) {
		tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
	}
	else {
		tft.setTextColor(ILI9341_BLACK, ILI9341_WHITE);
	}
	tft.setCursor(0, i*8);
	tft.print(r.name);

	/* UNCOMMENT THIS TO SEE A RATING FROM 1 TO 10 BESIDE ALL RESTAURANTS
	tft.print(" ");
	tft.print(r.rating);
	*/
}

// Begin mode 1 by sorting the restaurants around the cursor
// and then displaying the list.
void beginMode1() {
	tft.setCursor(0, 0);
	tft.fillScreen(ILI9341_BLACK);

	// Get the RestDist information for this cursor position and sort it.
	getAndSortRestaurants(curView, restaurants, &card, &cache);

	// Initially have the closest restaurant highlighted.
	selectedRest = 0;

	int restaurantsDisplayed = 0;

	for (int i = 0; i < NUM_RESTAURANTS && restaurantsDisplayed < REST_DISP_NUM; ++i)
	{
		// Temporaty restaurant
		restaurant r;

		// get the i'th restaurant
		getRestaurant(&r, restaurants[i].index, &card, &cache);

		if(r.rating >= selectedRating*2 || (r.rating == 1 && selectedRating == 1))
		{
			restPage[restaurantsDisplayed] = i;
			printRestaurant(r, restaurantsDisplayed);
			restaurantsDisplayed++;
		}
	}

	restsDisplayed = restaurantsDisplayed;

	// will null the rest of the page if less than 30 restaurants are displayed
	while(restaurantsDisplayed < REST_DISP_NUM)
	{
		restPage[restaurantsDisplayed] = 0;
		restaurantsDisplayed++;
	}

	mode = 1;
}

// load the next page of restaurants if the user scrolls. Similar to beginMode1()
void fetchNextPage(int startIndex)
{
	tft.setCursor(0, 0);
	tft.fillScreen(ILI9341_BLACK);

	// Initially have the closest restaurant highlighted.
	selectedRest = 0;

	int restaurantsDisplayed = 0;

	for (int i = startIndex; i < NUM_RESTAURANTS && restaurantsDisplayed < REST_DISP_NUM; ++i)
	{
		// Temporaty restaurant
		restaurant r;

		// get the i'th restaurant
		getRestaurant(&r, restaurants[i].index, &card, &cache);

		if(r.rating >= selectedRating*2 || (r.rating == 1 && selectedRating == 1))
		{
			restPage[restaurantsDisplayed] = i;
			printRestaurant(r, restaurantsDisplayed);

			restaurantsDisplayed++;
		}
	}
	restsDisplayed = restaurantsDisplayed;

	// will null the rest of the page if less than 30 restaurants are displayed
	while(restaurantsDisplayed < REST_DISP_NUM)
	{
		restPage[restaurantsDisplayed] = 0;
		restaurantsDisplayed++;
	}
}

// this function will retrieve the previous page of restaurants when the user scrolls to the top
// essentially fetchNextPage() in reverse
void fetchPrevPage(int endIndex)
{
	tft.setCursor(0, 0);
	tft.fillScreen(ILI9341_BLACK);

	// Initially have the closest restaurant highlighted.
	selectedRest = 0;

	int restaurantsDisplayed = REST_DISP_NUM - 1;

	for (int i = endIndex; i >= 0 && restaurantsDisplayed >= 0; --i)
	{
		// Temporaty restaurant
		restaurant r;

		// get the i'th restaurant
		getRestaurant(&r, restaurants[i].index, &card, &cache);

		if(r.rating >= selectedRating*2 || (r.rating == 1 && selectedRating == 1))
		{
			restPage[restaurantsDisplayed] = i;
			printRestaurant(r, restaurantsDisplayed);

			restaurantsDisplayed--;
		}
	}
	restsDisplayed = REST_DISP_NUM;

	// will null the rest of the page if less than 30 restaurants are displayed
	while(restaurantsDisplayed >= 0)
	{
		restPage[restaurantsDisplayed] = 0;
		restaurantsDisplayed--;
	}
}

// Checks if the edge was nudged and scrolls the map if so.
void checkRedrawMap() {
  // A flag to indicate if we scrolled.
	bool scroll = false;

	// If we nudged the left or right edge, shift the map over half a screen.
	if (curView.cursorX == DISP_WIDTH-CURSOR_SIZE/2-1 && curView.mapX != MAPWIDTH - DISP_WIDTH) {
		curView.mapX += DISP_WIDTH;
		curView.cursorX = DISP_WIDTH/2;
		scroll = true;
	}
	else if (curView.cursorX == CURSOR_SIZE/2 && curView.mapX != 0) {
		 curView.mapX -= DISP_WIDTH;
		 curView.cursorX = DISP_WIDTH/2;
		 scroll = true;
	}

	// If we nudges the top or bottom edge, shift the map up or down half a screen.
	if (curView.cursorY == DISP_HEIGHT-CURSOR_SIZE/2-1 && curView.mapY != MAPHEIGHT - DISP_HEIGHT) {
		curView.mapY += DISP_HEIGHT;
		curView.cursorY = DISP_HEIGHT/2;
		scroll = true;
	}
	else if (curView.cursorY == CURSOR_SIZE/2 && curView.mapY != 0) {
		curView.mapY -= DISP_HEIGHT;
		curView.cursorY = DISP_HEIGHT/2;
		scroll = true;
	}

	// If we nudged the edge, recalculate and draw the new rectangular portion of Edmonton to display.
	if (scroll) {
		// Make sure we didn't scroll outside of the map.
		curView.mapX = constrain(curView.mapX, 0, MAPWIDTH - DISP_WIDTH);
		curView.mapY = constrain(curView.mapY, 0, MAPHEIGHT - DISP_HEIGHT);

		lcd_image_draw(&edmontonBig, &tft, curView.mapX, curView.mapY, 0, 0, DISP_WIDTH, DISP_HEIGHT);
	}
}

// Process joystick input when in mode 0.
void scrollingMap() {
  int v = analogRead(JOY_VERT_ANALOG);
  int h = analogRead(JOY_HORIZ_ANALOG);
  int invSelect = digitalRead(JOY_SEL);

	// A flag to indicate if the cursor moved or not.
	bool cursorMove = false;

  // If there was vertical movement, then move the cursor.
  if (abs(v - JOY_CENTRE) > JOY_DEADZONE) {
    // First move the cursor.
    int delta = (v - JOY_CENTRE) / JOY_STEPS_PER_PIXEL;
		// Clamp it so it doesn't go outside of the screen.
    curView.cursorY = constrain(curView.cursorY + delta, CURSOR_SIZE/2, DISP_HEIGHT-CURSOR_SIZE/2-1);
		// And now see if it actually moved.
		cursorMove |= (curView.cursorY != preView.cursorY);
  }

	// If there was horizontal movement, then move the cursor.
  if (abs(h - JOY_CENTRE) > JOY_DEADZONE) {
    // Ideas are the same as the previous if statement.
    int delta = -(h - JOY_CENTRE) / JOY_STEPS_PER_PIXEL;
    curView.cursorX = constrain(curView.cursorX + delta, CURSOR_SIZE/2, DISP_WIDTH-CURSOR_SIZE/2-1);
		cursorMove |= (curView.cursorX != preView.cursorX);
  }

	// If the cursor actually moved.
	if (cursorMove) {
		// Check if the map edge was nudged, and move it if so.
		checkRedrawMap();

		preView.mapX = curView.mapX;
		preView.mapY = curView.mapY;

		// Now draw the cursor's new position.
		moveCursor();
	}

	preView = curView;

	// Did we click the button?
  if(invSelect == LOW){
		beginMode1();
    mode = 1;
    Serial.println(mode);
    Serial.println("MODE changed.");

		// Just to make sure the restaurant is not selected by accident
		// because the button was pressed too long.
		while (digitalRead(JOY_SEL) == LOW) { delay(10); }
  }

	// Handle the rating selector:
	handleRatingSelector();
}

// Process joystick movement when in mode 1.
void scrollingMenu() {
	int oldRest = selectedRest;

	int v = analogRead(JOY_VERT_ANALOG);

	// if the joystick was pushed up or down, change restaurants accordingly.
	if (v > JOY_CENTRE + JOY_DEADZONE) {
		++selectedRest;
	}
	else if (v < JOY_CENTRE - JOY_DEADZONE) {
		--selectedRest;
	}

	// if the cursor moves to the top or bottom of the screen, change the page if possible
	if(selectedRest >= REST_DISP_NUM && restPage[REST_DISP_NUM] < NUM_RESTAURANTS)
	{
		pageNumber++;
		fetchNextPage(restPage[oldRest]);
		oldRest = 0;
	}
	else if(selectedRest < 0 && pageNumber > 0)
	{
		pageNumber--;
		fetchPrevPage(restPage[oldRest]);
		//printRestaurant(restPage[selectedRest]);
		oldRest = 0;
	}

	selectedRest = constrain(selectedRest, 0, restsDisplayed - 1);

	// If we picked a new restaurant, update the way it and the previously
	// selected restaurant are displayed.
	if (oldRest != selectedRest) {
		printRestaurant(oldRest);
		printRestaurant(selectedRest);
		delay(50); // so we don't scroll too fast
	}

	// If we clicked on a restaurant.
	if (digitalRead(JOY_SEL) == LOW) {
		restaurant r;
		getRestaurant(&r, restaurants[restPage[selectedRest]].index, &card, &cache);
		// Calculate the new map view.

		// Center the map view at the restaurant, constraining against the edge of
		// the map if necessary.
		curView.mapX = constrain(lon_to_x(r.lon)-DISP_WIDTH/2, 0, MAPWIDTH-DISP_WIDTH);
		curView.mapY = constrain(lat_to_y(r.lat)-DISP_HEIGHT/2, 0, MAPHEIGHT-DISP_HEIGHT);

		// Draw the cursor, clamping to an edge of the map if needed.
		curView.cursorX = constrain(lon_to_x(r.lon) - curView.mapX, CURSOR_SIZE/2, DISP_WIDTH-CURSOR_SIZE/2-1);
		curView.cursorY = constrain(lat_to_y(r.lat) - curView.mapY, CURSOR_SIZE/2, DISP_HEIGHT-CURSOR_SIZE/2-1);

		preView = curView;

		beginMode0();

		// Ensures a long click of the joystick will not register twice.
		while (digitalRead(JOY_SEL) == LOW) { delay(10); }
	}
}

int main() {
	setup();

	// All the implementation work is done now, just have a loop that processes
	// joystick movement!
	while (true) {
		if (mode == 0) {
			scrollingMap();
		}
		else {
			scrollingMenu();
		}
	}

	Serial.end();
	return 0;
}
