#include <Arduino.h>
#include "helperfunctions.h"
#include <ezTime.h>
#include <ESP8266WiFi.h>

#include <ESP8266mDNS.h> // Include the mDNS library
//#include <ESP8266SSDP.h>

#include <FastLED.h>

//Webserver
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <ArduinoJson.h>

// How many leds are in the strip?
#define NUM_LEDS 60

// rotate leds by this number. Led ring is connected upside down
#define ROTATE_LEDS 30
#define MaxBrightness 255
#define MinBrightness 15
// For led chips like WS2812, which have a data line, ground, and power, you just
// need to define DATA_PIN.  For led chipsets that are SPI based (four wires - data, clock,
// ground, and power), like the LPD8806 define both DATA_PIN and CLOCK_PIN
// Clock pin only needed for SPI based chipsets when not using hardware SPI
#define DATA_PIN 3

#define LIGHTSENSORPIN A0 //Ambient light sensor reading

#define EXE_INTERVAL 5000
unsigned long lastExecutedMillis = 0; // vairable to save the last executed time
unsigned long currentMillis;

float lightReading;

// Config
String jsonConfigfile;

struct Config
{

	int showms;
	int showseconds;
	int autobrightness;
	int rainbowgb;
	char tz[64];
	char ssid[33];
	char wifipassword[65];
	char hostname[17];
};

typedef struct Clockdisplay
{
	int hourColor;
	int minuteColor;
	int secondColor;
	int backgroundColor;
	int hourMarkColor;
	int showms;
	int showseconds;
} clockdisplay;

clockdisplay clockdisplays[4]; //create an array of 5 clock displays

/* // Begin
#include

typedef struct {
int minimum;
int maximum;
int start;
int pin;
Servo device;
} DOF;

DOF dsof[3];

void setup() {
dsof[0] = {45, 180, 90, 2};
dsof[1] = {0, 180, 0, 3};
dsof[2] = {0, 180, 0, 4};

for (int i = 0; i < 3; i ++) {
dsof[i].device.attach(dsof[i].pin);
}
}
// End */

Config config; // <- global configuration object

// This is an array of leds.  One item for each led in your strip.
CRGB leds[NUM_LEDS];

Timezone tz;

///////////////////Webserver
int sliderBrightnessValue = 128;
const char *PARAM_INPUT = "value";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Replaces placeholder with section in your web page
String processor(const String &var)
{
	//Serial.println(var);
	if (var == "sliderBrightnessValue")
	{
		return String(sliderBrightnessValue);
	}
	if (var == "LDRValue")
	{
		return String(lightReading);
	}
	if (var == "backGroundColor")
	{

		return String(inttoHex(clockdisplays[0].backgroundColor, 6));
	}
	if (var == "hourMarkColor")
	{
		return String(inttoHex(clockdisplays[0].hourMarkColor, 6));
	}
	if (var == "hourColor")
	{
		return String(inttoHex(clockdisplays[0].hourColor, 6));
	}
	if (var == "minuteColor")
	{
		return String(inttoHex(clockdisplays[0].minuteColor, 6));
	}
	if (var == "secondColor")
	{
		//return String(inttoHex(config.secondColor, 6));
		return String(inttoHex(clockdisplays[0].secondColor, 6));
	}
	if (var == "showms")
	{
		if (config.showms != 0)
		{
			return String("checked");
		}
	}
	if (var == "showseconds")
	{
		if (config.showseconds != 0)
		{
			return String("checked");
		}
	}
	if (var == "autobrightness")
	{
		if (config.autobrightness != 0)
		{
			return String("checked");
		}
	}
	if (var == "ssid")
	{
		return String(config.ssid);
	}
	if (var == "wifipassword")
	{
		return String(config.wifipassword);
	}
	if (var == "hostname")
	{
		return String(config.hostname);
	}
	if (var == "tz")
	{
		return String(config.tz);
	}
	return String();
}

// Saves the configuration to a file
void saveConfiguration(const char *filename, const Config &config)
{
	// Delete existing file, otherwise the configuration is appended to the file
	SPIFFS.remove(filename);

	// Open file for writing
	File configfile = SPIFFS.open("/config.json", "w");

	if (!configfile)
	{
		Serial.println(F("Failed to create file"));
		return;
	}

	// Allocate a temporary JsonDocument
	// Don't forget to change the capacity to match your requirements.
	// Use arduinojson.org/assistant to compute the capacity.
	StaticJsonDocument<1024> doc;

	// Set the values in the document

	doc["backgroundcolor0"] = clockdisplays[0].backgroundColor;
	doc["hourmarkcolor0"] = clockdisplays[0].hourMarkColor;
	doc["hourcolor0"] = clockdisplays[0].hourColor;
	doc["minutecolor0"] = clockdisplays[0].minuteColor;
	doc["secondcolor0"] = clockdisplays[0].secondColor;
	/* doc["backgroundcolor1"] = clockdisplays[1].backgroundColor;
	doc["hourmarkcolor1"] = clockdisplays[1].hourMarkColor;
	doc["hourcolor1"] = clockdisplays[1].hourColor;
	doc["minutecolor1"] = clockdisplays[1].minuteColor;
	doc["secondcolor1"] = clockdisplays[1].secondColor;
	doc["backgroundcolor2"] = clockdisplays[2].backgroundColor;
	doc["hourmarkcolor2"] = clockdisplays[2].hourMarkColor;
	doc["hourcolor2"] = clockdisplays[2].hourColor;
	doc["minutecolor2"] = clockdisplays[2].minuteColor;
	doc["secondcolor2"] = clockdisplays[2].secondColor;
	doc["backgroundcolor3"] = clockdisplays[3].backgroundColor;
	doc["hourmarkcolor3"] = clockdisplays[3].hourMarkColor;
	doc["hourcolor3"] = clockdisplays[3].hourColor;
	doc["minutecolor3"] = clockdisplays[3].minuteColor;
	doc["secondcolor3"] = clockdisplays[3].secondColor;
	doc["backgroundcolor4"] = clockdisplays[4].backgroundColor;
	doc["hourmarkcolor4"] = clockdisplays[4].hourMarkColor;
	doc["hourcolor4"] = clockdisplays[4].hourColor;
	doc["minutecolor4"] = clockdisplays[4].minuteColor;
	doc["secondcolor4"] = clockdisplays[4].secondColor; */
	doc["showms"] = config.showms;
	doc["showseconds"] = config.showseconds;
	doc["autobrightness"] = config.autobrightness;
	doc["rainbowbg"] = config.rainbowgb;
	doc["tz"] = config.tz;
	doc["ssid"] = config.ssid;
	doc["wifipassword"] = config.wifipassword;
	doc["hostname"] = config.hostname;

	Serial.print(config.tz);

	// Serialize JSON to file
	if (serializeJson(doc, configfile) == 0)
	{
		Serial.println(F("Failed to write to file"));
	}

	// Close the file
	configfile.close();
}

int rotate(int nr)
{

	if ((nr + ROTATE_LEDS) >= 60)
	{

		return (nr + ROTATE_LEDS) - NUM_LEDS;
	}
	else
	{

		return (nr + ROTATE_LEDS);
	}
}

boolean checkConnection()
{
	int count = 0;
	Serial.print("Waiting for Wi-Fi connection");
	while (count < 30)
	{
		leds[rotate(count)] = CRGB::White;
		FastLED.show();

		if (WiFi.status() == WL_CONNECTED)
		{
			Serial.println();
			Serial.println("Connected!");
			leds[rotate(count)] = CRGB::Green;
			FastLED.show();
			return (true);
		}
		delay(500);
		Serial.print(".");
		count++;
	}
	Serial.println("Timed out.");
	leds[rotate(count)] = CRGB::Red;
	FastLED.show();
	delay(5000);
	return false;
}

void setup()
{

	// sanity check delay - allows reprogramming if accidently blowing power w/leds
	delay(2000);

	Serial.begin(115200);
	while (!Serial)
	{
		;
	} // wait for Serial port to connect. Needed for native USB port only

	// Initialize SPIFFS
	if (!SPIFFS.begin())
	{
		Serial.println("An Error has occurred while mounting SPIFFS");
		return;
	}

	////// Config //////
	File configfile = SPIFFS.open("/config.json", "r");

	if (!configfile)
	{
		Serial.println("file open failed");
	}

	// Allocate a temporary JsonDocument
	// Don't forget to change the capacity to match your requirements.
	// Use arduinojson.org/assistant to compute the capacity.
	StaticJsonDocument<512> doc;

	jsonConfigfile = (configfile.readString());

	// Deserialize the JSON document
	DeserializationError error = deserializeJson(doc, jsonConfigfile);
	if (error)
		Serial.println(F("Failed to read file, using default configuration"));

	configfile.close(); //close file

	//config.backgroundColor = doc["backgroundcolor"] | 0x120500;
	clockdisplays[0].backgroundColor = doc["backgroundcolor0"] | 0x120500;

	//config.hourMarkColor = doc["hourmarkcolor"] | 0x0F0F0F;
	clockdisplays[0].hourMarkColor = doc["hourmarkcolor0"] | 0x120500;

	//config.hourColor = doc["hourcolor"] | 0xFF0000;
	clockdisplays[0].hourColor = doc["hourcolor0"] | 0xFF0000;

	//config.minuteColor = doc["minutecolor"] | 0xFF0000;
	clockdisplays[0].minuteColor = doc["minutecolor0"] | 0xFF0000;

	//config.secondColor = doc["secondcolor"] | 0xFF0000;
	clockdisplays[0].secondColor = doc["secondcolor0"] | 0xFF0000;

	config.showms = doc["showms"] | 0;
	config.showseconds = doc["showseconds"] | 1;
	config.autobrightness = doc["autobrightness"] | 1;
	config.rainbowgb = doc["rainbowbg"] | 0;
	strlcpy(config.tz, doc["tz"] | "UTC", sizeof(config.tz));
	strlcpy(config.ssid, doc["ssid"] | "", sizeof(config.ssid));
	strlcpy(config.wifipassword, doc["wifipassword"] | "", sizeof(config.wifipassword));
	strlcpy(config.hostname, doc["hostname"] | "ledclock", sizeof(config.hostname));

	Serial.println("<<<<<< Clock configuration >>>>>>>");
	Serial.println("Background HEX color: " + inttoHex(clockdisplays[0].backgroundColor, 6));
	Serial.println("Hour mark HEX color: " + inttoHex(clockdisplays[0].hourMarkColor, 6));
	Serial.println("Show ms led: " + String(config.showms));
	Serial.println("Timezone: " + String(config.tz));
	Serial.println("ssid: " + String(config.ssid));
	//Serial.println("wifipassword: xxxxxxx"); // + String(config.wifipassword));
	Serial.println("hostname: " + String(config.hostname));

	Serial.println(jsonConfigfile);

	FastLED.setBrightness(128);
	FastLED.setMaxPowerInVoltsAndMilliamps(5, 1000); //max 1 amperé power usage

	FastLED.addLeds<SK6812, DATA_PIN, GRB>(leds, NUM_LEDS) // GRB ordering is typical
		.setCorrection(TypicalLEDStrip);
	//.setCorrection(0xE6E0E6);
	FastLED.setMaxRefreshRate(400);

	WiFi.mode(WIFI_STA);

	WiFi.hostname("ledclock");
	WiFi.begin(config.ssid, config.wifipassword);

	// Uncomment the line below to see what it does behind the scenes
	//setDebug(INFO);
	if (checkConnection())
	{

		//When there is a wifi connection get time from NTP
		waitForSync();

		Serial.println("UTC: " + UTC.dateTime());

		if (!MDNS.begin(config.hostname))
		{ // Start the mDNS responder for esp8266.local
			Serial.println("Error setting up MDNS responder!");
		}
		else
		{
			Serial.println("mDNS responder started");
		}
	}
	else
	{
		WiFi.disconnect();
		Serial.println("starting AP");
		//default IP = 192.168.4.1
		WiFi.softAP("ledclock", "letmein!");
	}

	// Provide official timezone names
	// https://en.wikipedia.org/wiki/List_of_tz_database_time_zones

	//Set timezone
	//tz.setCache(0);
	//tz.setLocation(config.tz);

	if (!tz.setCache(0))
		tz.setLocation(config.tz);

	Serial.println(String(config.tz) + ": " + tz.dateTime());

	// Webserver
	// Route for root / web page
	server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
		//request->send_P(200, "text/html", index_html, processor);
		request->send(SPIFFS, "/index.htm", String(), false, processor);
	});
	server.serveStatic("/mdg-ledr.js", SPIFFS, "/mdg-ledr.js", "max-age=31536000");
	server.serveStatic("/mdg-ledr.css", SPIFFS, "mdg-ledr.css", "max-age=31536000");
	server.serveStatic("/mdi-font.ttf", SPIFFS, "/mdi-font.ttf", "max-age=31536000");
	server.serveStatic("/mdi-font.woff", SPIFFS, "/mdi-font.woff", "max-age=31536000");
	server.serveStatic("/mdi-font.woff2", SPIFFS, "/mdi-font.woff2", "max-age=31536000");

	server.on("/save", HTTP_GET, [](AsyncWebServerRequest *request) { //save settings
		saveConfiguration("/config.json", config);
		request->send(200, "text/plain", "OK");
	});

	server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request) { //reboot ESP
		request->send(200, "text/plain", "OK");
		delay(500);
		ESP.restart();
	});
	server.on("/color", HTTP_GET, [](AsyncWebServerRequest *request) {
		String inputMessage;

		if (request->hasParam("hourcolor"))
		{
			inputMessage = request->getParam("hourcolor")->value();

			//config.hourColor = hstol(inputMessage);
			clockdisplays[0].hourColor = hstol(inputMessage);
		}
		else if (request->hasParam("minutecolor"))
		{
			inputMessage = request->getParam("minutecolor")->value();

			//config.minuteColor = hstol(inputMessage);
			clockdisplays[0].minuteColor = hstol(inputMessage);
		}
		else if (request->hasParam("secondcolor"))
		{
			inputMessage = request->getParam("secondcolor")->value();
			//config.secondColor = hstol(inputMessage);
			clockdisplays[0].secondColor = hstol(inputMessage);
		}
		else if (request->hasParam("backgroundcolor"))
		{
			inputMessage = request->getParam("backgroundcolor")->value();
			//config.backgroundColor = hstol(inputMessage);
			clockdisplays[0].backgroundColor = hstol(inputMessage);
		}
		else if (request->hasParam("hourmarkcolor"))
		{
			inputMessage = request->getParam("hourmarkcolor")->value();
			//config.hourMarkColor = hstol(inputMessage);
			clockdisplays[0].hourMarkColor = hstol(inputMessage);
		}
		else
		{
			inputMessage = "No message sent";
		}
		request->send(200, "text/plain", "OK");
	});

	server.on("/configuration", HTTP_GET, [](AsyncWebServerRequest *request) {
		int paramsNr = request->params();
		//Serial.println(paramsNr);

		for (int i = 0; i < paramsNr; i++)
		{

			AsyncWebParameter *p = request->getParam(i);

			//Serial.print("Param name: ");
			//Serial.println(p->name());
			//Serial.print("Param value: ");
			//Serial.println(p->value());
			//Serial.println("------");

			if (p->name() == "brightness")
			{
				//Serial.println("brightness");
				if (config.autobrightness == 0)
				{
					int NumtToBrightness = map(p->value().toInt(), 0, 255, MinBrightness, MaxBrightness);
					FastLED.setBrightness(NumtToBrightness);
				}
			}
			else if (p->name() == "showms")
			{
				config.showms = p->value().toInt();
			}
			else if (p->name() == "showseconds")
			{
				config.showseconds = p->value().toInt();
			}
			else if (p->name() == "autobrightness")
			{
				config.autobrightness = p->value().toInt();
			}
			else if (p->name() == "rainbowbg")
			{
				config.rainbowgb = p->value().toInt();
			}
			else if (p->name() == "ssid")
			{

				p->value().toCharArray(config.ssid, sizeof(config.ssid));
			}
			else if (p->name() == "wifipassword")
			{

				p->value().toCharArray(config.wifipassword, sizeof(config.wifipassword));
			}
			else if (p->name() == "hostname")
			{

				p->value().toCharArray(config.hostname, sizeof(config.hostname));
			}
			else if (p->name() == "tz")
			{
				tz.clearCache();

				p->value().toCharArray(config.tz, sizeof(config.tz));
			}
		}

		request->send(200, "text/plain", "OK");
	});

	MDNS.addService("http", "tcp", 80);

	// Start ElegantOTA
	AsyncElegantOTA.begin(&server);

	// Start server
	server.begin();
}

void loop()
{

	AsyncElegantOTA.loop();

	MDNS.update();

	for (int Led = 0; Led < 60; Led = Led + 1)
	{

		leds[Led] = clockdisplays[0].backgroundColor;
	}

	if (config.rainbowgb == 1) //show rainbow background
	{
		fill_rainbow(leds, NUM_LEDS, 5, 8);
	}

	for (int Led = 0; Led <= 55; Led = Led + 5)
	{

		leds[rotate(Led)] = clockdisplays[0].hourMarkColor;
		//leds[rotate(Led)].fadeToBlackBy(230);
	}

	int houroffset = map(tz.minute(), 0, 60, 0, 5); //move the hour hand when the minutes pass

	uint8_t hrs12 = (tz.hour() % 12);
	leds[rotate((hrs12 * 5) + houroffset)] = clockdisplays[0].hourColor;
	//leds[rotate((hrs12 * 5) + houroffset) + 1 ] = CRGB::DarkRed;
	//leds[rotate((hrs12 * 5) + houroffset )- 1 ] = CRGB::DarkRed;

	//Minute hand
	leds[rotate(tz.minute())] = clockdisplays[0].minuteColor;

	//Second hand
	if (config.showseconds == 1)
	{

		/* int msto255 = map(tz.ms(), 1, 1000, 0, 255);
		CRGB pixelColor1 = blend( config.secondColor, leds[rotate(tz.second()-1)] , msto255);
		CRGB pixelColor2 = blend( leds[rotate(tz.second())], config.secondColor, msto255);

		leds[rotate(tz.second())] = pixelColor2;
		leds[rotate(tz.second()-1)] = pixelColor1;   */

		//normal
		//leds[rotate(tz.second())] = config.secondColor;
		leds[rotate(tz.second())] = clockdisplays[0].secondColor;
	}

	if (config.showms == 1)
	{
		int ms = map(tz.ms(), 1, 1000, 0, 59);

		//leds[rotate(ms)] = config.secondColor;
		leds[rotate(ms)] = clockdisplays[0].secondColor;
	}

	currentMillis = millis();

	if (config.autobrightness == 1)
	{

		//Set brightness when auto brightness is active
		if (currentMillis - lastExecutedMillis >= EXE_INTERVAL)
		{
			lastExecutedMillis = currentMillis;		   // save the last executed time
			lightReading = analogRead(LIGHTSENSORPIN); //Read light level

			int brightnessMap = map(lightReading, 0, 64, MinBrightness, MaxBrightness);
			brightnessMap = constrain(brightnessMap, 0, 255);

			sliderBrightnessValue = brightnessMap;
			//int brightnessMap = map(lightReading, 0, 1024, MinBrightness, MaxBrightness);

			FastLED.setBrightness(brightnessMap);

			//Serial.println(lightReading);
			//Serial.println(brightnessMap);
		}
	}

	FastLED.show();
}