#include <libaldl.h>
#include <math.h>
#include <pigpio.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "VG/openvg.h"
#include "VG/vgu.h"

#include <fontinfo.h>
#include <shapes.h>

#define PI 3.14159265358979323

typedef struct {
    float x;
    float y;
} Point;

typedef struct {
    Point start;
    Point end;
} LineStruct;

void drawRoundPercent(Point origin, int radius, int percent);
void drawMeter(Point center, int radius, int angleStart, int angleEnd, int current, int max, int interval, char* unit, int redLine);
void drawGraduations(int arcRadius, int max, int interval, Point arcOrigin, int startDegrees, int endDegrees);
void drawStatusBar(Point statusOrigin, float width, float height, int fuelLevel, int trip, int range);
void drawInfoCenter(int errors[], int errorLength, Point infoCenterOrigin, int width, int height);
void drawOdometer(int miles, float mpgInst, float mpgAvg, Point odometerOrigin, float width, float height);
void drawSpeedLimit(int speedLimit);

float valueToAngle(int value, int valueMax, int angleStart, int angleEnd);
Point getPointOnCircle(int radius, float angleInDegrees, Point origin);
LineStruct getLineAcrossAngle(float angleInDegrees, int length, Point onCircle, Point circleOrigin, int radius);

int * getBitErrors(byte bytes[], int start, int length, int codes[5][8], int * errorSize);
int bitValue(byte num, int bit);

void getOil();
void getFuel();
void getBlinker();
