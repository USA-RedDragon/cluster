#include "cluster.h"

#include <libftdi1/ftdi.h>

struct ftdi_context *ftdi;

int aldl_ftdi_connect(int vendor, int device) {
	int ret;

	if((ftdi = ftdi_new()) == 0) {
		fprintf(stderr, "ftdi_new failed\n");
		return -1;
	}

	if((ret = ftdi_usb_open(ftdi, vendor, device)) < 0) {
		fprintf(stderr, "unable to open ftdi device: %d (%s)\n", ret, ftdi_get_error_string(ftdi));
		return -1;
	}

	ftdi_set_baudrate(ftdi, 8192);
	ftdi_set_latency_timer(ftdi, 2);

	return 0;
}

int aldl_send_message(byte* command, int length) {
	ftdi_usb_purge_buffers(ftdi);
	return ftdi_write_data(ftdi, (unsigned char *)command, length);
}

int aldl_receive_message(byte* buffer, int length) {
	int response = 0;
	response = ftdi_read_data(ftdi,(unsigned char *)buffer,length);
	return response;
}

void aldl_ftdi_disconnect() {
	ftdi_free(ftdi);
}

void aldl_ftdi_flush() {
	ftdi_usb_purge_buffers(ftdi);
}

typedef struct {
	double odometer;
	double trip;
	int range;
} savedData;

FILE *saveFile;

savedData data;

//Mode 1 command
byte ecm_mode1[] = {0xF4, 0x57, 0x01, 0x00, 0xB4};

//Mode 1 data
byte ecm_mode1_data[72];

int malfBits[5][8] = {
			{21, 1, 1, 1, 1, 15, 14, 13},
			{1, 28, 1, 1, 25, 24, 23, 22},
			{38, 37, 1, 35, 34, 33, 32, 31},
			{1, 46, 45, 44, 43, 42, 41, 39},
			{1, 55, 54, 53, 52, 51, 1, 1}
		};

unsigned long long int mpgTotal = 0;
unsigned long mpgSamples = 1;

int sigEnd=0;

//150 ms loop
struct timespec scanTime = {0, 150 * 1000000};

void sig_handler(int signo) {
	if (signo == SIGINT) {
		printf("Shutting Down...\n");
		sigEnd=1;
	}
}


int w, h;

char* errorValues[57] = {
	"Code 0: N/A",
	"Code 1: N/A",
	"Code 2: N/A",
	"Code 3: N/A",
	"Code 4: N/A",
	"Code 5: N/A",
	"Code 6: N/A",
	"Code 7: N/A",
	"Code 8: N/A",
	"Code 9: N/A",
	"Code 10: N/A",
	"Code 11: N/A",
	"Code 12: N/A",
	"Code 13: Oxygen Sensor",
	"Code 14: Coolant Sesnor High Temperature",
	"Code 15: Coolant Sensor Low Temperature",
	"Code 16: N/A",
	"Code 17: N/A",
	"Code 18: N/A",
	"Code 19: N/A",
	"Code 20: N/A",
	"Code 21: Throttle Position High",
	"Code 22: Throttle Position Low",
	"Code 23: MAT Sensor Low",
	"Code 24: Vehicle Speed Sensor",
	"Code 25: MAT Sensor High",
	"Code 26: N/A",
	"Code 27: N/A",
	"Code 28: Pressure Switch Manifold",
	"Code 29: N/A",
	"Code 30: N/A",
	"Code 31: Governor Failure",
	"Code 32: EGR Failure",
	"Code 33: MAP Sesnor High",
	"Code 34: MAP Sensor Low",
	"Code 35: IAC Failure",
	"Code 36: N/A",
	"Code 37: Brake On",
	"Code 38: Brake Off",
	"Code 39: TCC Off",
	"Code 40: N/A",
	"Code 41: 1X (CAM Pulse) Sensor Failure",
	"Code 42: EST. Monitor",
	"Code 43: ESC Failure",
	"Code 44: Oxygen Sensor Lean",
	"Code 45: Oxygen Sensor Rich",
	"Code 46: VATS Failure",
	"Code 47: N/A",
	"Code 48: N/A",
	"Code 49: N/A",
	"Code 50: N/A",
	"Code 51: PROM Error",
	"Code 52: System Voltage High-Long Test",
	"Code 53: System Voltage High",
	"Code 54: Fuel Pump Relay Malfunction",
	"Code 55: ADU Error",
	"Code 56: N/A"
};

int left_blinker = 0;
int right_blinker = 0;
int rounding = 20;

struct timespec oilStart={0,0}, oilEnd={0,0};
int oil = 0;

struct timespec fuelStart={0,0}, fuelEnd={0,0};
int fuelLevel = 100;

void saveData() {
	saveFile = fopen("/home/pi/.cluster.dat","w+");
	fwrite(&data, sizeof(data), 1, saveFile);
	fclose(saveFile);
}

void loadData() {
	saveFile = fopen("/home/pi/.cluster.dat","r");
	fread(&data, sizeof(data), 1, saveFile);
	fclose(saveFile);
}

/*void tripCallback(int gpio, int level, uint32_t tick) {
	if (level == 0) {
		data.trip = 0;
		gpioDelay(2000);
	}
}

void rangeCallback(int gpio, int level, uint32_t tick) {
	if (level == 0) {
		fuelLevel = 100;
		data.range = 25/(((float)mpgTotal)/mpgSamples)*100;
		gpioDelay(2000);
	}
}

void emergencyRebootCallback(int gpio, int level, uint32_t tick) {
	if (level == 0) {
        	saveData();
	        finish();
        	gpioTerminate();
	        aldl_ftdi_disconnect();
		sync();
		reboot(RB_AUTOBOOT);
		exit(0);
	}
}

void oilCallback(int gpio, int level, uint32_t tick) {
	if (level == 1) {
		clock_gettime(CLOCK_MONOTONIC, &oilEnd);
		unsigned long timeToCharge = oilEnd.tv_nsec - oilStart.tv_nsec;
		oil = timeToCharge*40/19800;
	}
}

void fuelCallback(int gpio, int level, uint32_t tick) {
	if (level == 1) {
		clock_gettime(CLOCK_MONOTONIC, &fuelEnd);
		unsigned long timeToCharge = fuelEnd.tv_nsec - fuelStart.tv_nsec;
		fuelLevel = timeToCharge*100/19800;
	}
}*/

int main() {
	init(&w, &h);
	Start(w, h);

	int rpm = 0;
	const int rpmMax = 5000;
	Point rpmArcOrigin = {w/4.0, h-h/3.0};
	int rpmArcRadius = h/5.4;
	int rpmArcAngleStart = 90;
	int rpmArcAngleEnd = 180;

	int water = 0;
	const int waterMax = 300;
	Point waterArcOrigin = {w-w/4.0, h-h/3.0};
	int waterArcRadius = h/5.4;
	int waterArcAngleStart = 90;
	int waterArcAngleEnd = 0;

	const int oilMax = 40;
	Point oilArcOrigin = {w/4.0-rpmArcRadius/2.0, h/4.0};
	int oilArcRadius = h/5.4;
	int oilArcAngleStart = 90;
	int oilArcAngleEnd = 0;

	int battery = 0;
	const int batteryMax = 16;
	Point batteryArcOrigin = {w-w/4.0+rpmArcRadius/2.0, h/4.0};
	int batteryArcRadius = h/5.4;
	int batteryArcAngleStart = 90;
	int batteryArcAngleEnd = 180;

	int mph = 0;
	const int mphMax = 140;
	Point mphArcOrigin = {w/2.0, h-h/2.0};
	int mphArcRadius = h/3.6;
	int mphArcAngleStart = 0;
	int mphArcAngleEnd = 180;

	Point statusOrigin = {w*0.25, h-h*0.12};
	float statusBarHeight = h*0.075;
	float statusBarWidth = w*0.5;

	Point infoCenterOrigin = {w/4, h*0.05};
	float infoCenterHeight = h*0.07;
	float infoCenterWidth = w/2;

	Point odometerOrigin = {w/2-w/8, infoCenterOrigin.y+h*0.10};
	float odometerWidth = w/4;
	float odometerHeight = h*0.066;

	if (signal(SIGINT, sig_handler) == SIG_ERR) {
		return 1;
	}

	if(aldl_ftdi_connect(0x0403, 0x6001) != 0) {
		return 1;
	}

	/*if (gpioInitialise() < 0) {
		printf("pigpio initialisation failed\n");
		return 1;
	}

	gpioSetMode(18, PI_OUTPUT); //Fuel Out
	gpioSetMode(23, PI_INPUT); //Fuel In

	gpioSetMode(17, PI_OUTPUT); //Oil Out
	gpioSetMode(22, PI_INPUT); //Oil In

	gpioSetMode(25, PI_INPUT); //Right Blinker
	gpioSetMode(9, PI_INPUT); //Left Blinker

	gpioSetMode(11, PI_INPUT); //Trip Reset
	gpioSetMode(8, PI_INPUT); //Mileage Reset
	gpioSetMode(1, PI_INPUT); //Reboot

	gpioSetPullUpDown(11, PI_PUD_UP);
	gpioSetPullUpDown(8, PI_PUD_UP);
	gpioSetPullUpDown(1, PI_PUD_UP);

	gpioSetAlertFunc(11, tripCallback);
	gpioSetAlertFunc(8, rangeCallback);
	gpioSetAlertFunc(1, emergencyRebootCallback);
	*/

	loadData();

	while(sigEnd == 0) {
		aldl_ftdi_flush();
		if((aldl_send_message(ecm_mode1, 5)) < 0) {
			aldl_ftdi_disconnect();
			return 1;
		}

		nanosleep(&scanTime, NULL);

		aldl_receive_message(ecm_mode1_data, 72);

		//add 7 to which offset you want in data
		rpm = ecm_mode1_data[41]*25;
		mph = ecm_mode1_data[38];
		float bpw = (ecm_mode1_data[64]*256+ecm_mode1_data[65])/65.536;
		float mpg = ((((double)mph)/3600))/(((bpw/1000*0.122222)/6.07)*(((double)rpm)/60));
		if(mpg > 0) {
			mpgTotal += ((int) mpg);
			mpgSamples++;
		}
		battery = ((float)ecm_mode1_data[23])/10;
		water = ecm_mode1_data[22]*1.35-40;

		int * errorSize = (int *)0;
		int * errors = getBitErrors(ecm_mode1_data, 13, 5, malfBits, errorSize);

		data.trip += mph/3600000.0*150;
		data.range = 25/(((float)mpgTotal)/mpgSamples)*100-data.trip; //25 gallon tank/avgMpg
		data.odometer += data.trip;

		nanosleep(&scanTime, NULL);
		Background(0, 0, 0);
		getBlinker();
		getFuel();
		getOil();
		drawStatusBar(statusOrigin, statusBarWidth, statusBarHeight, fuelLevel, data.trip, data.range);
		drawInfoCenter(errors, (int) errorSize, infoCenterOrigin, infoCenterWidth, infoCenterHeight);
		drawOdometer(data.odometer, mpg, ((float)mpgTotal)/mpgSamples, odometerOrigin, odometerWidth, odometerHeight);
		//drawSpeedLimit(65);
		drawMeter(rpmArcOrigin, rpmArcRadius, rpmArcAngleStart, rpmArcAngleEnd, rpm, rpmMax, 1000, "RPM", 4000);
		drawMeter(oilArcOrigin, oilArcRadius, oilArcAngleStart, oilArcAngleEnd, oil, oilMax, 10, "PSI", 35);
		drawMeter(mphArcOrigin, mphArcRadius, mphArcAngleStart, mphArcAngleEnd, mph, mphMax, 5, "MPH", 0);
		drawMeter(waterArcOrigin, waterArcRadius, waterArcAngleStart, waterArcAngleEnd, water, waterMax, 50, "° F", 250);
		drawMeter(batteryArcOrigin, batteryArcRadius, batteryArcAngleStart, batteryArcAngleEnd, battery, batteryMax, 4, "Volts", 14);
		End();
	}

	saveData();
	finish();
	//gpioTerminate();
	aldl_ftdi_disconnect();

	return 0;
}

void drawRoundPercent(Point origin, int radius, int percent) {
	char value[3];
	snprintf(value, 3, "%d", percent);

	StrokeWidth(4.0);
	//Green Stroke
	Stroke(0, 125, 0, 1);

	ArcOutline(origin.x, origin.y, radius, radius, 90, 3.6*percent);

	static int pointSize=0;
	if(percent > 9) {
		for(int i=1; i<40; i++) {
			if(TextWidth(value, SerifTypeface, i) > radius) {
				pointSize = i-3;
				break;
			}
		}
	}

	if(percent != 100)
		TextMid(origin.x, origin.y-pointSize/2, value, SerifTypeface, pointSize);
}

void drawMeter(Point center, int radius, int angleStart, int angleEnd, int current, int max, int interval, char* unit, int redLine) {
	char valueStr[5];
	snprintf(valueStr, 5, "%d", current);

	//Green fill
	Fill(0, 125, 0, 1);

	Circle(center.x, center.y, 15);

	StrokeWidth(2.0);
	//Green Stroke
	Stroke(0, 125, 0, 1);

	//right is 90, Top is 180, Right is 270, Bottom is 0
	ArcOutline(center.x, center.y, radius*2, radius*2, angleStart, angleEnd-angleStart);

	drawGraduations(radius, max, interval, center, angleStart, angleEnd);

	float angle = valueToAngle(current, max, angleStart, angleEnd);

	StrokeWidth(3.0);
	//Orange Stroke
	Stroke(255, 106, 51, 1);
	Point gaugePoint = getPointOnCircle(radius, angle, center);
	LineStruct gaugeLine = getLineAcrossAngle(angle, radius, gaugePoint, center, radius);
	Line(gaugeLine.start.x, gaugeLine.start.y, gaugeLine.end.x, gaugeLine.end.y);

	StrokeWidth(2.0);
	//Green Stroke
	Stroke(0, 125, 0, 1);
	Point midPoint = getPointOnCircle(radius/2, valueToAngle(max/2, max, angleStart, angleEnd), center);
	TextMid(midPoint.x, midPoint.y, valueStr, SerifTypeface, h/54);
	TextMid(midPoint.x, midPoint.y-25, unit, SerifTypeface, h/54);

	if(redLine > 0) {
		Stroke(255, 0, 0, 1);
		float redAngle = valueToAngle(redLine, max, angleStart, angleEnd);
		ArcOutline(center.x, center.y, radius*2-2, radius*2-2, redAngle, angleStart-redAngle);
	}

	Stroke(0, 125, 0, 1);

}

void drawStatusBar(Point statusOrigin, float width, float height, int fuelLevel, int trip, int range) {
	StrokeWidth(2.0);
	//Green Stroke
	Stroke(0, 125, 0, 1);

	Point fuelArcOrigin = {w*0.5, statusOrigin.y+height/2};
	int fuelArcRadius = h/21.6;
	drawRoundPercent(fuelArcOrigin, fuelArcRadius, fuelLevel);

	RoundrectOutline(statusOrigin.x, statusOrigin.y, width, height, rounding, rounding);

	VGfloat* blinkerX = malloc(sizeof(VGfloat)*8);

	float blinkerXStart = statusOrigin.x+w*0.0104;
	float blinkerXFirstOffset = w*0.0208;
	float blinkerXEndOffset = w*0.0416;

	blinkerX[0] = blinkerXStart;
	blinkerX[1] = blinkerXStart + blinkerXFirstOffset;
	blinkerX[2] = blinkerXStart + blinkerXFirstOffset;
	blinkerX[3] = blinkerXStart + blinkerXEndOffset;
	blinkerX[4] = blinkerXStart + blinkerXEndOffset;
	blinkerX[5] = blinkerXStart + blinkerXFirstOffset;
	blinkerX[6] = blinkerXStart + blinkerXFirstOffset;
	blinkerX[7] = blinkerXStart;

	VGfloat* blinkerY = malloc(sizeof(VGfloat)*8);

        float blinkerYStart = statusOrigin.y+height/2;
        float blinkerYFirstOffset = h*0.0277;
        float blinkerYEndOffset = blinkerYFirstOffset/2;

	blinkerY[0] = blinkerYStart;
	blinkerY[1] = blinkerYStart+blinkerYFirstOffset;
	blinkerY[2] = blinkerYStart+blinkerYEndOffset;
	blinkerY[3] = blinkerYStart+blinkerYEndOffset;
	blinkerY[4] = blinkerYStart-blinkerYEndOffset;
	blinkerY[5] = blinkerYStart-blinkerYEndOffset;
	blinkerY[6] = blinkerYStart-blinkerYFirstOffset;
	blinkerY[7] = blinkerYStart;

	if(left_blinker != 0) {
		if(left_blinker == 1)
			Polygon(blinkerX, blinkerY, 8);
	}

	blinkerXStart = statusOrigin.x+width-w*0.0104;

	blinkerX[0] = blinkerXStart;
	blinkerX[1] = blinkerXStart - blinkerXFirstOffset;
	blinkerX[2] = blinkerXStart - blinkerXFirstOffset;
	blinkerX[3] = blinkerXStart - blinkerXEndOffset;
	blinkerX[4] = blinkerXStart - blinkerXEndOffset;
	blinkerX[5] = blinkerXStart - blinkerXFirstOffset;
	blinkerX[6] = blinkerXStart - blinkerXFirstOffset;
	blinkerX[7] = blinkerXStart;

	if(right_blinker != 0) {
		if(right_blinker == 1)
			Polygon(blinkerX, blinkerY, 8);
	}

	free(blinkerX);
	free(blinkerY);

	StrokeWidth(2.0);
	//Green Stroke
	Stroke(0, 125, 0, 1);

	time_t t = time(NULL);
	struct tm * timeinfo;
	timeinfo = localtime(&t);

	char* valueStr[2] = { malloc(sizeof(char)*12), malloc(sizeof(char)*9) };
	strftime(valueStr[0], 12, "%a, %b %d", timeinfo);
	strftime(valueStr[1], 9, "%I:%M %p", timeinfo);
	Text(statusOrigin.x-w*0.01-TextWidth(valueStr[0], SerifTypeface, height/2), statusOrigin.y+height/4, valueStr[0], SerifTypeface, height/2);
	Text(statusOrigin.x+w*0.01+width, statusOrigin.y+height/4, valueStr[1], SerifTypeface, height/2);

	free(valueStr[0]);
	free(valueStr[1]);

	//Trip
	char tripStr[7];
	snprintf(tripStr, 7, "%d mi", trip);

	Point tripOrigin = {statusOrigin.x+width/2-w*0.01-fuelArcRadius-TextWidth(tripStr, SerifTypeface, height/3), statusOrigin.y+w*0.005};

	float tripWidth = TextWidth(tripStr, SerifTypeface, height/3);
	float tripHeight = height-w*0.01;

	RectOutline(tripOrigin.x,
		tripOrigin.y,
		tripWidth,
		tripHeight);

	TextMid(tripOrigin.x+tripWidth/2,
		tripOrigin.y+tripHeight/2+w*0.0025,
		"Trip",
		SerifTypeface, height/3);

	TextMid(tripOrigin.x+tripWidth/2,
		tripOrigin.y+w*0.005,
		tripStr,
		SerifTypeface, height/3);

	//Range
	char rangeStr[7];
	snprintf(rangeStr, 7, "%d mi", range);

	Point rangeOrigin = {statusOrigin.x+width/2+w*0.01+fuelArcRadius, statusOrigin.y+w*0.005};

	float rangeWidth = TextWidth("Range", SerifTypeface, height/3);
	float rangeHeight = height-w*0.01;

	RectOutline(rangeOrigin.x,
		rangeOrigin.y,
		rangeWidth,
		rangeHeight);

	TextMid(rangeOrigin.x+rangeWidth/2,
		rangeOrigin.y+rangeHeight/2+w*0.0025,
		"Range",
		SerifTypeface, height/3);

	TextMid(rangeOrigin.x+rangeWidth/2,
		rangeOrigin.y+w*0.005,
		rangeStr,
		SerifTypeface, height/3);
}

void drawInfoCenter(int errors[], int errorLength, Point infoCenterOrigin, int width, int height) {
	RoundrectOutline(infoCenterOrigin.x, infoCenterOrigin.y, width, height, rounding, rounding);

	static unsigned long errorCount = 0;
	static unsigned long count = 0;
	count++;

	//Every 1.5 seconds
	if(count % 10 == 0) {
		errorCount++;
	}

	Stroke(0, 125, 0, 1);
	if(errorLength > 0 && errorLength !=1) {
		Text(infoCenterOrigin.x+h*0.015, infoCenterOrigin.y+height/4, errorValues[errors[errorCount % errorLength]], SerifTypeface, height*0.5);
	} else if(errorLength == 1) {
		Text(infoCenterOrigin.x+h*0.015, infoCenterOrigin.y+height/4, errorValues[errors[0]], SerifTypeface, height*0.5);
	} else {
		Text(infoCenterOrigin.x+h*0.015, infoCenterOrigin.y+height/4, "No Errors", SerifTypeface, height*0.5);
	}
	free(errors);
}

void drawOdometer(int miles, float mpgInst, float mpgAvg, Point odometerOrigin, float width, float height) {
	char milesStr[7], mpgInstStr[9], mpgAvgStr[9];
	snprintf(milesStr, 7, "%d", miles);
	snprintf(mpgInstStr, 9, "%.1f MPG", mpgInst);
	snprintf(mpgAvgStr, 9, "%.1f MPG", mpgAvg);

	RoundrectOutline(odometerOrigin.x, odometerOrigin.y, width, height, rounding, rounding);
	Line(odometerOrigin.x+width/3, odometerOrigin.y+height, odometerOrigin.x+width/3, odometerOrigin.y);
	Line(odometerOrigin.x+width/3*2, odometerOrigin.y+height, odometerOrigin.x+width/3*2, odometerOrigin.y);

	TextMid(odometerOrigin.x+width/2, odometerOrigin.y+height/2+h*0.015, "Odometer", SerifTypeface, height/4);
	TextMid(odometerOrigin.x+width/2, odometerOrigin.y+h*0.015, milesStr, SerifTypeface, height/3);

	Text(odometerOrigin.x+h*0.007, odometerOrigin.y+height/2+h*0.015, "MPG Instant", SerifTypeface, height/5);
	Text(odometerOrigin.x+h*0.007, odometerOrigin.y+h*0.015, mpgInstStr, SerifTypeface, height/4);

	Text(odometerOrigin.x+(width/3)*2+h*0.007, odometerOrigin.y+height/2+h*0.015, "MPG Average", SerifTypeface, height/6);
	Text(odometerOrigin.x+(width/3)*2+h*0.007, odometerOrigin.y+h*0.015, mpgAvgStr, SerifTypeface, height/4);
}

void drawSpeedLimit(int speedLimit) {
	char speedStr[4];
	snprintf(speedStr, 4, "%d", speedLimit);

	RoundrectOutline(w/2-w/20, h/2-h/4, w/10, 1.25*(w/10), rounding, rounding);
	TextMid((w/2-w/20)+w/10/2, h/2-h/4+1.25*(w/10)-1.25*(w/10)*0.09, "Speed Limit", SerifTypeface, 1.25*(w/10)*0.09);
	TextMid((w/2-w/20)+w/10/2, h/2-h/4+1.25*(w/10)/2, speedStr, SerifTypeface, 1.25*(w/10)*0.15);
	TextMid((w/2-w/20)+w/10/2, h/2-h/4+1.25*(w/10)/2-1.25*(w/10)*0.12, "MPH", SerifTypeface, 1.25*(w/10)*0.11);
}

Point getPointOnCircle(int radius, float angleInDegrees, Point origin) {
	float radians = angleInDegrees * PI / 180;
	Point ret;
	ret.x = origin.x + radius * cos(radians);
	ret.y = origin.y + radius * sin(radians);
	return ret;
}

float valueToAngle(int value, int valueMax, int angleStart, int angleEnd) {
	float deltaAngle = angleEnd-angleStart;
	float oneDegree = valueMax/((float)deltaAngle);
	return angleEnd-(value/oneDegree);
}

LineStruct getLineAcrossAngle(float angleInDegrees, int length, Point onCircle, Point circleOrigin, int radius) {
	Point point1 = { onCircle.x, onCircle.y };
	Point point2 = getPointOnCircle(radius-length, angleInDegrees, circleOrigin);
	LineStruct ret = { point1, point2 };
	return ret;
}

void drawGraduations(int arcRadius, int max, int interval, Point arcOrigin, int startDegrees, int endDegrees) {
	float intervalAngle = (endDegrees-startDegrees)/(((float)max)/interval);
	LineStruct line;
	Point text;

	for(int i=0; i < (max/interval)+1; i++) {
		line = getLineAcrossAngle(endDegrees-(intervalAngle*i), 10, getPointOnCircle(arcRadius, endDegrees-(intervalAngle*i), arcOrigin), arcOrigin, arcRadius);
		Line(line.start.x, line.start.y, line.end.x, line.end.y);
		text = getPointOnCircle(arcRadius-h*0.025, endDegrees-(intervalAngle*i), arcOrigin);
		char textStr[5];
		snprintf(textStr, 5, "%d", i*interval);
		TextMid(text.x, text.y-h*0.009, textStr, SerifTypeface, h/65);
	}
}

int * getBitErrors(byte bytes[], int start, int length, int codes[5][8], int * errorSize) {
	int numTrue = 0;
	for(int i=0; i < length; i++) {
		for(int x=1; x < 8; x++) {
			if(bitValue(bytes[i+start], x) == 1)
				numTrue++;
		}
	}
	if(numTrue > 0) {
		int * ret = malloc(numTrue*sizeof(int));
		int num = 0;
		for(int x=0; x < length; x++) {
			for(int y=1; y < 8; y++) {
				if(bitValue(bytes[x+start], y) == 1) {
					ret[++num] = codes[x][y];
				}
			}
		}
		*errorSize = numTrue;
		return ret;
	} else {
		int * ret = malloc(sizeof(int));
		ret[0] = -1;
		*errorSize = 1;
		return ret;
	}
}

int bitValue(byte num, int bit) {
	if (bit > 0 && bit <= 8)
		return ((num >> (bit-1)) & 1);
	else
		return 0;
}

/*void getBlinker() {
	right_blinker = gpioRead(25);
	left_blinker = gpioRead(9);
}

void getOil() {
	gpioSetAlertFunc(22, oilCallback);

        gpioSetMode(17, PI_INPUT);
        gpioSetMode(22, PI_OUTPUT);

	gpioWrite(22, PI_OFF); //Drain capacitor

	gpioDelay(2500);

	clock_gettime(CLOCK_MONOTONIC, &oilStart);

        gpioSetMode(17, PI_OUTPUT);
	gpioSetMode(22, PI_INPUT);

	gpioWrite(17, PI_ON); //Charge capacitor
}

void getFuel() {
	gpioSetAlertFunc(23, fuelCallback);

        gpioSetMode(18, PI_INPUT);
        gpioSetMode(23, PI_OUTPUT);

	gpioWrite(23, PI_OFF); //Drain capacitor

	gpioDelay(2500);

	clock_gettime(CLOCK_MONOTONIC, &fuelStart);

        gpioSetMode(18, PI_OUTPUT);
	gpioSetMode(23, PI_INPUT);

	gpioWrite(18, PI_ON); //Charge capacitor
}
*/
