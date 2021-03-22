#include <Arduino.h>
#include "helperfunctions.h"
#include <ezTime.h>
#include <ESP8266WiFi.h>

#include <ESP8266mDNS.h> // Include the mDNS library

#include <FastLED.h>

//Webserver
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <ArduinoJson.h>

#include "LittleFS.h" // LittleFS is declared

// How many leds are in the clock?
#define NUM_LEDS 60

// rotate leds by this number. Led ring is connected upside down
#define ROTATE_LEDS 30

// Max brightness
#define MaxBrightness 255
// Min brightness
#define MinBrightness 15

#define DATA_PIN 3

#define LIGHTSENSORPIN A0 //Ambient light sensor reading

//Interval to check light sensor value
#define EXE_INTERVAL_AUTO_BRIGHTNESS 5000
unsigned long lastExecutedMillis = 0; // variable to save the last executed time
unsigned long currentMillis;

boolean apStopped = false;
boolean wifiConnected = false;

float lightReading;

//Nr of user defined clock displays
#define CLOCK_DISPLAYS 8

//Nr of user defines schedules
#define SCHEDULES 12

// Config
String jsonConfigfile;
String jsonSchedulefile;
String jsonClkdisplaysfile;

String responseSSID;

struct Config
{
	int activeclockdisplay;
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
	int autobrightness;
	int brightness;
	int rainbowgb;
} clockdisplay;

typedef struct Schedule
{
	int hour;
	int minute;
	int activeclockdisplay;
	int active;

} schedule;

schedule schedules[SCHEDULES];

int scheduleId = 0;
int currentMinute;

clockdisplay clockdisplays[CLOCK_DISPLAYS]; //Array of clock displays

Config config; // <- global configuration object

tmElements_t tm;

// This is an array of leds.  One item for each led in the clock
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
		return String(inttoHex(clockdisplays[config.activeclockdisplay].backgroundColor, 6));
	}
	if (var == "hourMarkColor")
	{
		return String(inttoHex(clockdisplays[config.activeclockdisplay].hourMarkColor, 6));
	}
	if (var == "hourColor")
	{
		return String(inttoHex(clockdisplays[config.activeclockdisplay].hourColor, 6));
	}
	if (var == "minuteColor")
	{
		return String(inttoHex(clockdisplays[config.activeclockdisplay].minuteColor, 6));
	}
	if (var == "secondColor")
	{

		return String(inttoHex(clockdisplays[config.activeclockdisplay].secondColor, 6));
	}
	if (var == "showms")
	{
		if (clockdisplays[config.activeclockdisplay].showms != 0)
		{
			return String("checked");
		}
	}
	if (var == "showseconds")
	{
		if (clockdisplays[config.activeclockdisplay].showseconds != 0)
		{
			return String("checked");
		}
	}
	if (var == "autobrightness")
	{
		if (clockdisplays[config.activeclockdisplay].autobrightness != 0)
		{
			return String("checked");
		}
	}
	if (var == "activeclockdisplay")
	{
		return String(config.activeclockdisplay);
	}
	if (var == "ssid")
	{
		return String(config.ssid);
	}
	if (var == "wifipassword")
	{

		if (String(config.wifipassword).isEmpty())
		{
			return String(config.wifipassword);
		}
		else
		{
			return String("********");
		}
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

void saveSchedules(const char *filename)
{
	// Delete existing file, otherwise the configuration is appended to the file
	LittleFS.remove(filename);

	File schedulefile = LittleFS.open(filename, "w");

	if (!schedulefile)
	{
		Serial.println(F("Failed to create file"));
		return;
	}

	// Allocate a temporary JsonDocument
	// Don't forget to change the capacity to match your requirements.
	// Use arduinojson.org/assistant to compute the capacity.

	DynamicJsonDocument data(1536);
	// Set the values in the document
	for (int i = 0; i <= SCHEDULES - 1; i++)
	{
		JsonObject obj = data.createNestedObject();
		obj["i"] = i;
		obj["a"] = schedules[i].active;
		obj["acd"] = schedules[i].activeclockdisplay;
		obj["h"] = schedules[i].hour;
		obj["m"] = schedules[i].minute;
		/* JsonArray levels = obj.createNestedArray("weekdays");
		levels.add(1);
		levels.add(2); */
	}

	// Serialize JSON to file
	if (serializeJson(data, schedulefile) == 0)
	{
		Serial.println(F("Failed to write to file"));
	}

	// Close the file
	schedulefile.close();
}

void saveConfiguration(const char *filename)
{
	// Delete existing file, otherwise the configuration is appended to the file
	LittleFS.remove(filename);

	// Open file for writing
	File configfile = LittleFS.open(filename, "w");

	if (!configfile)
	{
		Serial.println(F("Failed to create file"));
		return;
	}

	// Allocate a temporary JsonDocument
	// Don't forget to change the capacity to match your requirements.
	// Use arduinojson.org/assistant to compute the capacity.

	DynamicJsonDocument doc(2048);

	doc["acd"] = config.activeclockdisplay;
	doc["tz"] = config.tz;
	doc["ssid"] = config.ssid;
	doc["wp"] = config.wifipassword;
	doc["hn"] = config.hostname;

	Serial.print(config.tz);

	// Serialize JSON to file
	if (serializeJson(doc, configfile) == 0)
	{
		Serial.println(F("Failed to write to file"));
	}

	// Close the file
	configfile.close();
}

void saveClockDisplays(const char *filename)
{
	// Delete existing file, otherwise the configuration is appended to the file
	LittleFS.remove(filename);

	// Open file for writing
	File configfile = LittleFS.open(filename, "w");

	if (!configfile)
	{
		Serial.println(F("Failed to create file"));
		return;
	}

	// Allocate a temporary JsonDocument
	// Don't forget to change the capacity to match your requirements.
	// Use arduinojson.org/assistant to compute the capacity.

	DynamicJsonDocument data(2048);

	// Set the values in the document
	for (int i = 0; i <= CLOCK_DISPLAYS - 1; i++)

	{
		JsonObject obj = data.createNestedObject();
		obj["i"] = i;
		obj["bgc"] = clockdisplays[i].backgroundColor;
		obj["hmc"] = clockdisplays[i].hourMarkColor;
		obj["hc"] = clockdisplays[i].hourColor;
		obj["mc"] = clockdisplays[i].minuteColor;
		obj["sc"] = clockdisplays[i].secondColor;
		obj["ms"] = clockdisplays[i].showms;
		obj["s"] = clockdisplays[i].showseconds;
		obj["ab"] = clockdisplays[i].autobrightness;
		obj["bn"] = clockdisplays[i].brightness;
		obj["rb"] = clockdisplays[i].rainbowgb;
	}

	// Serialize JSON to file
	if (serializeJson(data, configfile) == 0)
	{
		Serial.println(F("Failed to write to file"));
	}

	// Close the file
	configfile.close();
}

//Rotate clock display
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

unsigned char bssid[6];
int channel;
int rssi = -999;

void scanWifi(String ssid)
{

	byte numSsid = WiFi.scanNetworks();
	rssi = -999;
	for (int thisNet = 0; thisNet < numSsid; thisNet++)
	{
		//Serial.println(WiFi.SSID(thisNet));
		if (WiFi.SSID(thisNet) == ssid)
		{
			Serial.println("found SSID: " + WiFi.SSID(thisNet));
			Serial.println("rssi: " + String(WiFi.RSSI(thisNet)));
			Serial.println("channel: " + String(WiFi.channel(thisNet)));
			if (WiFi.RSSI(thisNet) > rssi)
			{
				memcpy(bssid, WiFi.BSSID(thisNet), 6);
				channel = WiFi.channel(thisNet);
				rssi = WiFi.RSSI(thisNet);
			}
		}
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

	Serial.println("----------------- [MDG Ledclock] ------------------");
	// Initialize LittleFS
	if (!LittleFS.begin())
	{
		Serial.println("An Error has occurred while mounting LittleFS");
		return;
	}

	///// Config file /////
	File configfile = LittleFS.open("/config.json", "r");
	if (!configfile)
	{
		Serial.println("Config file open failed");
	}
	// Allocate a temporary JsonDocument
	// Don't forget to change the capacity to match your requirements.
	// Use arduinojson.org/assistant to compute the capacity.
	DynamicJsonDocument config_doc(256);

	jsonConfigfile = (configfile.readString());

	// Deserialize the JSON document
	DeserializationError error_config = deserializeJson(config_doc, jsonConfigfile);
	if (error_config)
		Serial.println(F("Failed to read config.json file, using default configuration"));

	configfile.close(); //close file

	///// Schedule file /////
	File schedulefile = LittleFS.open("/schedules.json", "r");
	if (!schedulefile)
	{
		Serial.println("Schedule file open failed");
	}
	// Allocate a temporary JsonDocument
	// Don't forget to change the capacity to match your requirements.
	// Use arduinojson.org/assistant to compute the capacity.
	DynamicJsonDocument schedule_doc(1536);

	jsonSchedulefile = (schedulefile.readString());

	// Deserialize the JSON document
	DeserializationError error_schedule = deserializeJson(schedule_doc, jsonSchedulefile);
	if (error_schedule)
		Serial.println(F("Failed to read schedule.json file, using default configuration"));

	schedulefile.close(); //close file

	for (int i = 0; i <= SCHEDULES - 1; i++)
	{
		schedules[i].active = schedule_doc[i]["a"] | 0;
		schedules[i].activeclockdisplay = schedule_doc[i]["acd"] | 0;
		schedules[i].hour = schedule_doc[i]["h"] | 0;
		schedules[i].minute = schedule_doc[i]["m"] | 0;
	}

	////// Config file //////
	File clkdisplaysfile = LittleFS.open("/clkdisplays.json", "r");

	if (!clkdisplaysfile)
	{
		Serial.println("Clock display file open failed");
	}

	// Allocate a temporary JsonDocument
	// Don't forget to change the capacity to match your requirements.
	// Use arduinojson.org/assistant to compute the capacity.
	DynamicJsonDocument clkdisplays_doc(2048);

	jsonClkdisplaysfile = (clkdisplaysfile.readString());

	// Deserialize the JSON document
	DeserializationError error_clkdisplays = deserializeJson(clkdisplays_doc, jsonClkdisplaysfile);
	if (error_clkdisplays)
		Serial.println(F("Failed to read file clkdisplays.json, using default configuration"));

	clkdisplaysfile.close(); //close file

	for (int i = 0; i <= CLOCK_DISPLAYS - 1; i++)
	{

		clockdisplays[i].backgroundColor = clkdisplays_doc[i]["bgc"] | 0x002622;
		clockdisplays[i].hourMarkColor = clkdisplays_doc[i]["hmc"] | 0xb81f00;
		clockdisplays[i].hourColor = clkdisplays_doc[i]["hc"] | 0xffe3e9;
		clockdisplays[i].minuteColor = clkdisplays_doc[i]["mc"] | 0xffe3e9;
		clockdisplays[i].secondColor = clkdisplays_doc[i]["sc"] | 0xffe3e9;
		clockdisplays[i].showms = clkdisplays_doc[i]["ms"] | 0;
		clockdisplays[i].showseconds = clkdisplays_doc[i]["s"] | 1;
		clockdisplays[i].autobrightness = clkdisplays_doc[i]["ab"] | 1;
		clockdisplays[i].brightness = clkdisplays_doc[i]["bn"] | 128;
		clockdisplays[i].rainbowgb = clkdisplays_doc[i]["rb"] | 0;
	}

	config.activeclockdisplay = config_doc["acd"] | 0;

	strlcpy(config.tz, config_doc["tz"] | "UTC", sizeof(config.tz));
	strlcpy(config.ssid, config_doc["ssid"] | "", sizeof(config.ssid));
	strlcpy(config.wifipassword, config_doc["wp"] | "", sizeof(config.wifipassword));
	strlcpy(config.hostname, config_doc["hn"] | "ledclock", sizeof(config.hostname));

	/*
	Serial.println(jsonSchedulefile); 	
	Serial.println(jsonConfigfile);
	Serial.println(jsonClkdisplaysfile); */

	FastLED.setBrightness(128);
	FastLED.setMaxPowerInVoltsAndMilliamps(5, 1000); //max 1 amperé power usage

	FastLED.addLeds<SK6812, DATA_PIN, GRB>(leds, NUM_LEDS) // GRB ordering is typical
		//	.setCorrection(TypicalLEDStrip);
		.setCorrection(0xE6E0E6);
	FastLED.setMaxRefreshRate(400);

	WiFi.mode(WIFI_AP_STA);

	WiFi.hostname(config.hostname);

	scanWifi(config.ssid);

	if (rssi != -999) //connect to strongest ap
	{
		WiFi.begin(config.ssid, config.wifipassword, channel);
		//Serial.println("Connected to channel: " + String(channel));
	}
	else
	{
		WiFi.begin(config.ssid, config.wifipassword);
	}

	// Uncomment the line below to see what it does behind the scenes
	//setDebug(INFO);
	if (checkConnection())
	{
		wifiConnected = true;
		//When there is a wifi connection get time from NTP
		waitForSync();
	}
	else
	{
		wifiConnected = false;

		//WiFi.disconnect();

		//WiFi.softAPdisconnect (true); //Disconnect softAP
	}

	byte mac[6];

	WiFi.macAddress(mac);

	Serial.println("----------------- [Information] ------------------");
	Serial.print("MAC address: ");
	Serial.println(WiFi.macAddress());
	String apPassword = String(mac[5], HEX) + String(mac[4], HEX) + String(mac[3], HEX) + String(mac[2] + mac[5], HEX);
	Serial.println("AP Password: " + apPassword);
	Serial.println("Hostname: " + String(config.hostname));
	Serial.println("SSID: " + WiFi.SSID());
	Serial.println("rssi: " + String(WiFi.RSSI()));
	Serial.println("Channel: " + String(channel));
	Serial.println("IP Address: " + WiFi.localIP().toString());

	//Serial.println("starting AP");
	//default IP = 192.168.4.1
	//WiFi.mode(WIFI_AP);

	if (wifiConnected == true)
	{
		WiFi.softAP("MDG-Ledclock1 " + WiFi.localIP().toString(), apPassword); //show ipadress when connected to wifi
	}
	else
	{
		WiFi.softAP("MDG-Ledclock1", apPassword);
	}

	if (!MDNS.begin(config.hostname))
	{ // Start the mDNS responder for esp8266.local
		Serial.println("Error setting up MDNS responder!");
	}

	// Provide official timezone names
	// https://en.wikipedia.org/wiki/List_of_tz_database_time_zones
	if (!tz.setCache(0))
		tz.setLocation(config.tz);

	Serial.println("Time zone: " + String(config.tz));
	Serial.println("---------------------------------------------------");

	// Webserver
	// Route for root / web page
	server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
		//request->send_P(200, "text/html", index_html, processor);
		request->send(LittleFS, "/index.htm", String(), false, processor);
	});
	server.serveStatic("/mdg-ledr.js", LittleFS, "/mdg-ledr.js", "max-age=31536000");
	server.serveStatic("/mdg-ledr.css", LittleFS, "mdg-ledr.css", "max-age=31536000");
	server.serveStatic("/mdi-font.ttf", LittleFS, "/mdi-font.ttf", "max-age=31536000");
	server.serveStatic("/mdi-font.woff", LittleFS, "/mdi-font.woff", "max-age=31536000");
	server.serveStatic("/mdi-font.woff2", LittleFS, "/mdi-font.woff2", "max-age=31536000");

	server.on("/save-config", HTTP_GET, [](AsyncWebServerRequest *request) { //save config
		Serial.println("save");

		saveConfiguration("/config.json");
		request->send(200, "text/plain", "OK");
	});

	server.on("/scan-networks", HTTP_GET, [](AsyncWebServerRequest *request) { //save config
		Serial.println("** Scan Networks **");

		WiFi.scanNetworksAsync([request](int numNetworks) {
			DynamicJsonDocument data(2048);
			int o = numNetworks;
			int loops = 0;

			if (numNetworks == 0)
				Serial.println("no networks found");
			else
			{
				// sort by RSSI
				int indices[numNetworks];
				int skip[numNetworks];

				String ssid;

				for (int i = 0; i < numNetworks; i++)
				{
					indices[i] = i;
				}

				// CONFIG
				bool sortRSSI = true;	// sort aps by RSSI
				bool removeDups = true; // remove dup aps ( forces sort )
				bool printAPs = true;	// print found aps

				bool printAPFound = false;	 // do home ap check
				const char *homeAP = "MYAP"; // check for this ap on each scan
				// --------

				bool homeAPFound = false;

				if (removeDups || sortRSSI)
				{
					for (int i = 0; i < numNetworks; i++)
					{
						for (int j = i + 1; j < numNetworks; j++)
						{
							if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i]))
							{
								loops++;
								std::swap(indices[i], indices[j]);
								std::swap(skip[i], skip[j]);
							}
						}
					}
				}

				if (removeDups)
				{
					for (int i = 0; i < numNetworks; i++)
					{
						if (indices[i] == -1)
						{
							--o;
							continue;
						}
						ssid = WiFi.SSID(indices[i]);
						for (int j = i + 1; j < numNetworks; j++)
						{
							loops++;
							if (ssid == WiFi.SSID(indices[j]))
							{
								indices[j] = -1;
							}
						}
					}
				}

				//    Serial.println((String)loops);
				Serial.print(o);
				Serial.println(" networks found of " + (String)numNetworks);

				Serial.println("00: (RSSI)[BSSID][hidden] SSID [channel] [encryption]");
				for (int i = 0; i < numNetworks; ++i)
				{
					if (printAPFound && (WiFi.SSID(indices[i]) == homeAP))
						homeAPFound = true;

					if (printAPs && indices[i] != -1)
					{

						JsonObject obj = data.createNestedObject();
						obj["i"] = i;
						obj["ssid"] = WiFi.SSID(indices[i]);

						// Print SSID and RSSI for each network found
						Serial.printf("%02d", i + 1);
						Serial.print(":");

						Serial.print(" (");
						Serial.print(WiFi.RSSI(indices[i]));
						Serial.print(")");

						Serial.print(" [");
						Serial.print(WiFi.BSSIDstr(indices[i]));
						Serial.print("]");

						Serial.print(" [");
						Serial.print((String)WiFi.isHidden(indices[i]));
						Serial.print("]");

						Serial.print(" " + WiFi.SSID(indices[i]));
						// Serial.print((WiFi.encryptionType(indices[i]) == ENC_TYPE_NONE)?" ":"*");

						Serial.print(" [");
						Serial.printf("%02d", (int)WiFi.channel(indices[i]));
						Serial.print("]");

						Serial.print(" [");
						Serial.print((String)encryptionTypeStr(WiFi.encryptionType(indices[i])));
						Serial.print("]");

						//      Serial.print(" WiFi index: " + (String)indices[i]);

						Serial.println();
					}
					delay(10);
				}
				if (printAPFound && !homeAPFound)
					Serial.println("HOME AP NOT FOUND");
				Serial.println("");
			}
			String response;
			serializeJson(data, response);
			request->send(200, "application/json", response);
			Serial.println(ESP.getFreeHeap());
		});

	});

	server.on("/save-clockdisplays", HTTP_GET, [](AsyncWebServerRequest *request) { //save clockdisplays
		saveClockDisplays("/clkdisplays.json");
		request->send(200, "text/plain", "OK");
	});
	server.on("/get-clockdisplay", HTTP_GET, [](AsyncWebServerRequest *request) {
		StaticJsonDocument<256> data;
		if (request->hasParam("id"))
		{
			int i = (request->getParam("id")->value()).toInt();

			data["bgc"] = String(inttoHex(clockdisplays[i].backgroundColor, 6));
			data["hmc"] = String(inttoHex(clockdisplays[i].hourMarkColor, 6));
			data["hc"] = String(inttoHex(clockdisplays[i].hourColor, 6));
			data["mc"] = String(inttoHex(clockdisplays[i].minuteColor, 6));
			data["sc"] = String(inttoHex(clockdisplays[i].secondColor, 6));
			data["ms"] = clockdisplays[i].showms;
			data["s"] = clockdisplays[i].showseconds;
			data["ab"] = clockdisplays[i].autobrightness;
			data["bn"] = clockdisplays[i].brightness;
			data["rb"] = clockdisplays[i].rainbowgb;
		}
		else
		{
			data["message"] = "No clockdisplay id";
		}
		String response;
		serializeJson(data, response);
		request->send(200, "application/json", response);
	});
	server.on("/get-schedule", HTTP_GET, [](AsyncWebServerRequest *request) {
		StaticJsonDocument<256> data;
		if (request->hasParam("id"))
		{

			int i = (request->getParam("id")->value()).toInt();
			if (i <= SCHEDULES)
			{
				data["a"] = schedules[i].active;
				data["acd"] = schedules[i].activeclockdisplay;
				data["h"] = schedules[i].hour;
				data["m"] = schedules[i].minute;
			}
			else
			{
				data["message"] = "Schedule not found";
			}
		}
		else
		{
			data["message"] = "No Schedule id";
		}
		String response;
		serializeJson(data, response);
		request->send(200, "application/json", response);
	});
	server.on("/get-schedules", HTTP_GET, [](AsyncWebServerRequest *request) {
		StaticJsonDocument<1536> data;

		for (int i = 0; i <= SCHEDULES - 1; i++)
		{
			JsonObject obj = data.createNestedObject();
			obj["i"] = i;
			obj["a"] = schedules[i].active;
			obj["acd"] = schedules[i].activeclockdisplay;
			obj["h"] = schedules[i].hour;
			obj["m"] = schedules[i].minute;
		}

		String response;
		serializeJson(data, response);
		request->send(200, "application/json", response);
	});
	server.on("/set-schedule", HTTP_GET, [](AsyncWebServerRequest *request) {
		int paramsNr = request->params();

		for (int i = 0; i < paramsNr; i++)
		{
			AsyncWebParameter *p = request->getParam(i);

			if (p->name() == "id")
			{
				scheduleId = p->value().toInt();
			}
			else if (p->name() == "hour")
			{
				schedules[scheduleId].hour = p->value().toInt();
				Serial.println(scheduleId);
			}
			else if (p->name() == "minute")
			{
				schedules[scheduleId].minute = p->value().toInt();
			}
			else if (p->name() == "active")
			{
				schedules[scheduleId].active = p->value().toInt();
			}
			else if (p->name() == "activeclockdisplay")
			{
				schedules[scheduleId].activeclockdisplay = p->value().toInt();
			}
		}
		saveSchedules("/schedules.json");

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
			clockdisplays[config.activeclockdisplay].hourColor = hstol(inputMessage);
		}
		else if (request->hasParam("minutecolor"))
		{
			inputMessage = request->getParam("minutecolor")->value();
			clockdisplays[config.activeclockdisplay].minuteColor = hstol(inputMessage);
		}
		else if (request->hasParam("secondcolor"))
		{
			inputMessage = request->getParam("secondcolor")->value();
			clockdisplays[config.activeclockdisplay].secondColor = hstol(inputMessage);
		}
		else if (request->hasParam("backgroundcolor"))
		{
			inputMessage = request->getParam("backgroundcolor")->value();
			clockdisplays[config.activeclockdisplay].backgroundColor = hstol(inputMessage);
		}
		else if (request->hasParam("hourmarkcolor"))
		{
			inputMessage = request->getParam("hourmarkcolor")->value();
			clockdisplays[config.activeclockdisplay].hourMarkColor = hstol(inputMessage);
		}
		else
		{
			inputMessage = "No message sent";
		}
		request->send(200, "text/plain", "OK");
	});
	server.on("/configuration", HTTP_GET, [](AsyncWebServerRequest *request) {
		int paramsNr = request->params();

		for (int i = 0; i < paramsNr; i++)
		{
			AsyncWebParameter *p = request->getParam(i);

			if (p->name() == "brightness")
			{
				if (clockdisplays[config.activeclockdisplay].autobrightness == 0)
				{
					clockdisplays[config.activeclockdisplay].brightness = p->value().toInt();

					int NumtToBrightness = map(clockdisplays[config.activeclockdisplay].brightness, 0, 255, MinBrightness, MaxBrightness);
					FastLED.setBrightness(NumtToBrightness);
				}
			}
			else if (p->name() == "showms")
			{
				clockdisplays[config.activeclockdisplay].showms = p->value().toInt();
			}
			else if (p->name() == "activeclockdisplay")
			{
				config.activeclockdisplay = p->value().toInt();
			}
			else if (p->name() == "showseconds")
			{
				clockdisplays[config.activeclockdisplay].showseconds = p->value().toInt();
			}
			else if (p->name() == "autobrightness")
			{
				clockdisplays[config.activeclockdisplay].autobrightness = p->value().toInt();
			}
			else if (p->name() == "rainbowbg")
			{
				clockdisplays[config.activeclockdisplay].rainbowgb = p->value().toInt();
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

	currentMillis = millis();

	events();

	AsyncElegantOTA.loop();

	MDNS.update();

	if (currentMillis > 300000 && apStopped == false && wifiConnected == true) //disable AP after 5 minutes but only when there is a Wi-Fi connection
	{

		WiFi.softAPdisconnect(true);
		Serial.println("Stopping AP...");
		apStopped = true;
	}

	//Process Schedule every new minute
	if (currentMinute != tz.minute()) //check if a minute has passed
	{

		currentMinute = tz.minute(); //set current minute
		//Serial.println("Check for event.....");
		//	breakTime(tz.now(), tm);
		for (int i = 0; i <= SCHEDULES - 1; i++)
		{
			if (tz.hour() == schedules[i].hour && tz.minute() == schedules[i].minute && schedules[i].active == 1)
			{
				config.activeclockdisplay = schedules[i].activeclockdisplay;
				Serial.println("Schedule active..");
			}
		}
	}

	//Set background color
	for (int Led = 0; Led < 60; Led = Led + 1)
	{
		leds[Led] = clockdisplays[config.activeclockdisplay].backgroundColor;
	}

	if (clockdisplays[config.activeclockdisplay].rainbowgb == 1) //show rainbow background
	{
		fill_rainbow(leds, NUM_LEDS, 5, 8);
	}

	//Set hour marks
	for (int Led = 0; Led <= 55; Led = Led + 5)
	{
		leds[rotate(Led)] = clockdisplays[config.activeclockdisplay].hourMarkColor;
		//leds[rotate(Led)].fadeToBlackBy(230);
	}

	//Hour hand
	int houroffset = map(tz.minute(), 0, 60, 0, 5); //move the hour hand when the minutes pass

	uint8_t hrs12 = (tz.hour() % 12);
	leds[rotate((hrs12 * 5) + houroffset)] = clockdisplays[config.activeclockdisplay].hourColor;
	//leds[rotate((hrs12 * 5) + houroffset) + 1 ] = CRGB::DarkRed;
	//leds[rotate((hrs12 * 5) + houroffset )- 1 ] = CRGB::DarkRed;

	//Minute hand
	leds[rotate(tz.minute())] = clockdisplays[config.activeclockdisplay].minuteColor;

	//Second hand
	if (clockdisplays[config.activeclockdisplay].showseconds == 1)
	{

		/* int msto255 = map(tz.ms(), 1, 1000, 0, 255);
		CRGB pixelColor1 = blend(clockdisplays[config.activeclockdisplay].secondColor, leds[rotate(tz.second() - 1)], msto255);
		CRGB pixelColor2 = blend(leds[rotate(tz.second())], clockdisplays[config.activeclockdisplay].secondColor, msto255);

		leds[rotate(tz.second())] = pixelColor2;
		leds[rotate(tz.second() - 1)] = pixelColor1;  */

		//normal

		leds[rotate(tz.second())] = clockdisplays[config.activeclockdisplay].secondColor;
	}

	if (clockdisplays[config.activeclockdisplay].showms == 1)
	{
		int ms = map(tz.ms(), 1, 1000, 0, 59);

		//leds[rotate(ms)] = config.secondColor;
		leds[rotate(ms)] = clockdisplays[config.activeclockdisplay].secondColor;
	}

	// Set Brightness
	if (clockdisplays[config.activeclockdisplay].autobrightness == 1)
	{
		//Set brightness when auto brightness is active
		if (currentMillis - lastExecutedMillis >= EXE_INTERVAL_AUTO_BRIGHTNESS)
		{
			lastExecutedMillis = currentMillis;		   // save the last executed time
			lightReading = analogRead(LIGHTSENSORPIN); //Read light level

			int brightnessMap = map(lightReading, 0, 128, MinBrightness, MaxBrightness);
			brightnessMap = constrain(brightnessMap, 0, 255);

			sliderBrightnessValue = brightnessMap;
			//int brightnessMap = map(lightReading, 0, 1024, MinBrightness, MaxBrightness);

			FastLED.setBrightness(brightnessMap);
			//Serial.println(lightReading);
			//Serial.println(brightnessMap);
		}
	}
	else
	{
		sliderBrightnessValue = clockdisplays[config.activeclockdisplay].brightness;

		int brightnessMap = map(sliderBrightnessValue, 0, 255, MinBrightness, MaxBrightness);
		brightnessMap = constrain(brightnessMap, 0, 255);

		FastLED.setBrightness(brightnessMap);
	}

	FastLED.show();
}